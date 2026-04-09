// Nimród Autofill – QWebChannel bridge alapú credential manager
// Betöltődik minden oldalba DocumentReady fázisban.

(function () {
    'use strict';

    // ── Segédfüggvények ────────────────────────────────────────────────────

    function getDomain() {
        return location.hostname || '';
    }

    function findPasswordInputs() {
        return Array.from(document.querySelectorAll('input[type="password"]'));
    }

    // Keres egy username mezőt a password input előtt / mellette
    function findUsernameInput(passwordInput) {
        // Keressük az ugyanabban a form-ban lévő text/email/tel inputot
        var form = passwordInput.closest('form');
        if (form) {
            var candidates = form.querySelectorAll(
                'input[type="text"], input[type="email"], input[type="tel"], input[autocomplete="username"], input[name*="user"], input[name*="email"], input[id*="user"], input[id*="email"]'
            );
            if (candidates.length > 0) return candidates[candidates.length - 1];
        }
        // Fallback: közeli input a DOM-ban
        var all = Array.from(document.querySelectorAll('input[type="text"], input[type="email"]'));
        var pwIdx = all.indexOf(passwordInput);
        if (pwIdx < 0) {
            all = Array.from(document.querySelectorAll('input'));
            var inputs = all.filter(function(i) { return i !== passwordInput; });
            for (var i = inputs.length - 1; i >= 0; i--) {
                if (inputs[i].compareDocumentPosition(passwordInput) & Node.DOCUMENT_POSITION_FOLLOWING)
                    return inputs[i];
            }
        }
        return all[Math.max(0, pwIdx - 1)] || null;
    }

    function showToast(message, isError) {
        var existing = document.getElementById('__nimrod_toast__');
        if (existing) existing.remove();

        var toast = document.createElement('div');
        toast.id = '__nimrod_toast__';
        toast.textContent = message;
        toast.style.cssText = [
            'position:fixed',
            'bottom:20px',
            'right:20px',
            'z-index:2147483647',
            'background:' + (isError ? '#c0392b' : '#27ae60'),
            'color:#fff',
            'padding:10px 18px',
            'border-radius:6px',
            'font:14px/1.4 sans-serif',
            'box-shadow:0 4px 12px rgba(0,0,0,0.3)',
            'transition:opacity 0.4s',
            'opacity:1'
        ].join(';');
        document.body.appendChild(toast);
        setTimeout(function() {
            toast.style.opacity = '0';
            setTimeout(function() { toast.remove(); }, 400);
        }, 2500);
    }

    // ── WebChannel init ────────────────────────────────────────────────────

    function initBridge(callback) {
        if (typeof qt === 'undefined' || !qt.webChannelTransport) {
            // QWebChannel nem elérhető ezen az oldalon
            return;
        }
        new QWebChannel(qt.webChannelTransport, function(channel) {
            var bridge = channel.objects.nimrodBridge;
            if (!bridge) return;
            callback(bridge);
        });
    }

    // ── Autofill ───────────────────────────────────────────────────────────

    function autofill(bridge) {
        var domain = getDomain();
        if (!domain) return;

        bridge.getCredentials(domain, function(json) {
            var creds;
            try { creds = JSON.parse(json); } catch(e) { return; }
            if (!creds || creds.length === 0) return;

            var first = creds[0];
            var pwInputs = findPasswordInputs();
            pwInputs.forEach(function(pwInput) {
                var userInput = findUsernameInput(pwInput);
                if (userInput && !userInput.value) {
                    userInput.value = first.username;
                    userInput.dispatchEvent(new Event('input', {bubbles: true}));
                    userInput.dispatchEvent(new Event('change', {bubbles: true}));
                }
                if (!pwInput.value) {
                    pwInput.value = first.password;
                    pwInput.dispatchEvent(new Event('input', {bubbles: true}));
                    pwInput.dispatchEvent(new Event('change', {bubbles: true}));
                }
            });
        });
    }

    // ── Form submit figyelő ────────────────────────────────────────────────

    function attachFormListeners(bridge) {
        var domain = getDomain();

        function captureForm(form) {
            if (form.__nimrod_listening__) return;
            form.__nimrod_listening__ = true;

            form.addEventListener('submit', function() {
                var pwInputs = form.querySelectorAll('input[type="password"]');
                pwInputs.forEach(function(pwInput) {
                    var userInput = findUsernameInput(pwInput);
                    var username = userInput ? userInput.value.trim() : '';
                    var password = pwInput.value;
                    if (username && password) {
                        bridge.saveCredential(domain, username, password);
                        showToast('🔐 Nimród: belépési adatok elmentve', false);
                    }
                });
            }, true);
        }

        // Meglévő formok
        document.querySelectorAll('form').forEach(captureForm);

        // Dinamikusan betöltött formok (SPA, pl. Google login)
        var observer = new MutationObserver(function(mutations) {
            mutations.forEach(function(m) {
                m.addedNodes.forEach(function(node) {
                    if (node.nodeType !== 1) return;
                    if (node.tagName === 'FORM') captureForm(node);
                    node.querySelectorAll && node.querySelectorAll('form').forEach(captureForm);
                });
            });
        });
        observer.observe(document.body || document.documentElement, {
            childList: true, subtree: true
        });
    }

    // ── Belépési pont ──────────────────────────────────────────────────────

    function main() {
        initBridge(function(bridge) {
            autofill(bridge);
            attachFormListeners(bridge);
        });
    }

    if (document.readyState === 'loading') {
        document.addEventListener('DOMContentLoaded', main);
    } else {
        main();
    }

})();
