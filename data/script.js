const HID_KEYCODES = {
    'a': 4, 'b': 5, 'c': 6, 'd': 7, 'e': 8, 'f': 9, 'g': 10, 'h': 11,
    'i': 12, 'j': 13, 'k': 14, 'l': 15, 'm': 16, 'n': 17, 'o': 18, 'p': 19,
    'q': 20, 'r': 21, 's': 22, 't': 23, 'u': 24, 'v': 25, 'w': 26, 'x': 27,
    'y': 28, 'z': 29,
    '1': 30, '2': 31, '3': 32, '4': 33, '5': 34, '6': 35, '7': 36, '8': 37,
    '9': 38, '0': 39,
    'Enter': 40, 'Escape': 41, 'Backspace': 42, 'Tab': 43, 'Space': 44,
    '-': 45, '=': 46, '[': 47, ']': 48, '\\': 49, ';': 51, "'": 52, '`': 53,
    ',': 54, '.': 55, '/': 56,
    'CapsLock': 57,
    'F1': 58, 'F2': 59, 'F3': 60, 'F4': 61, 'F5': 62, 'F6': 63,
    'F7': 64, 'F8': 65, 'F9': 66, 'F10': 67, 'F11': 68, 'F12': 69,
    'Insert': 73, 'Home': 74, 'PageUp': 75, 'Delete': 76, 'End': 77, 'PageDown': 78,
    'ArrowRight': 79, 'ArrowLeft': 80, 'ArrowDown': 81, 'ArrowUp': 82
};

let shiftPressed = false;
let lastKeyPress = 0;
const DEBOUNCE_MS = 50;
let statusUpdateInterval = null;

function createKeyboard() {
    const keyboard = document.getElementById('keyboard');
    keyboard.innerHTML = '';
    
    const layout = [
        ['`', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', 'Backspace'],
        ['Tab', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\\'],
        ['CapsLock', 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', "'", 'Enter'],
        ['Shift', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 'Shift'],
        ['Space']
    ];
    
    layout.forEach((row) => {
        const rowDiv = document.createElement('div');
        rowDiv.className = 'keyboard-row';
        
        row.forEach(key => {
            const btn = document.createElement('button');
            btn.className = 'btn btn-secondary keyboard-key';
            btn.textContent = key;
            btn.dataset.key = key.toLowerCase();
            btn.type = 'button';
            
            if (key === 'Shift') {
                const handleShiftStart = (e) => {
                    e.preventDefault();
                    shiftPressed = true;
                    btn.classList.add('active');
                };
                const handleShiftEnd = (e) => {
                    e.preventDefault();
                    shiftPressed = false;
                    btn.classList.remove('active');
                };
                
                btn.addEventListener('mousedown', handleShiftStart);
                btn.addEventListener('mouseup', handleShiftEnd);
                btn.addEventListener('mouseleave', handleShiftEnd);
                btn.addEventListener('touchstart', handleShiftStart, {passive: false});
                btn.addEventListener('touchend', handleShiftEnd, {passive: false});
                btn.addEventListener('touchcancel', handleShiftEnd, {passive: false});
            } else {
                const handleKeyPress = (e) => {
                    e.preventDefault();
                    sendKey(key);
                };
                btn.addEventListener('click', handleKeyPress);
                btn.addEventListener('touchend', handleKeyPress, {passive: false});
            }
            
            rowDiv.appendChild(btn);
        });
        
        keyboard.appendChild(rowDiv);
    });
}

function sendKey(key) {
    const now = Date.now();
    if (now - lastKeyPress < DEBOUNCE_MS) {
        return;
    }
    lastKeyPress = now;
    
    const keyLower = key.toLowerCase();
    let keycode = HID_KEYCODES[keyLower] || HID_KEYCODES[key];
    
    if (!keycode) {
        console.warn('Unknown key:', key);
        return;
    }
    
    let modifiers = 0;
    if (shiftPressed || (key >= 'A' && key <= 'Z' && key !== keyLower)) {
        modifiers = 0x02;
        if (keycode >= 4 && keycode <= 29) {
            keycode = HID_KEYCODES[keyLower];
        }
    }
    
    fetch('/api/key', {
        method: 'POST',
        headers: {
            'Content-Type': 'application/x-www-form-urlencoded',
        },
        body: `keycode=${keycode}&modifiers=${modifiers}`
    }).catch(err => {
        console.error('Error sending key:', err);
    });
}

function updateStatus() {
    fetch('/api/status')
        .then(response => {
            if (!response.ok) throw new Error('Network error');
            return response.json();
        })
        .then(data => {
            const statusIndicator = document.getElementById('bleStatus');
            const statusText = document.getElementById('statusText');
            const currentSlot = document.getElementById('currentSlot');
            
            if (data.connected) {
                statusIndicator.className = 'status-indicator status-connected';
                statusText.textContent = 'Connected';
            } else {
                statusIndicator.className = 'status-indicator status-disconnected';
                statusText.textContent = 'Disconnected';
            }
            
            currentSlot.textContent = data.slot;
            
            document.querySelectorAll('.slot-btn').forEach(btn => {
                if (parseInt(btn.dataset.slot) === data.slot) {
                    btn.classList.add('active');
                } else {
                    btn.classList.remove('active');
                }
            });
        })
        .catch(err => {
            console.error('Error fetching status:', err);
        });
}

document.querySelectorAll('.slot-btn').forEach(btn => {
    btn.addEventListener('click', () => {
        const slot = btn.dataset.slot;
        fetch('/api/slot', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/x-www-form-urlencoded',
            },
            body: `slot=${slot}`
        }).then(() => {
            setTimeout(updateStatus, 500);
        }).catch(err => {
            console.error('Error switching slot:', err);
        });
    });
});

createKeyboard();
updateStatus();
if (statusUpdateInterval) clearInterval(statusUpdateInterval);
statusUpdateInterval = setInterval(updateStatus, 2000);
