let ws;
let triggers = [];
let intervals = [];
let controls = [];

let div = document.getElementById("buttons");
let transportSlider = document.getElementById("transport");
transportSlider.addEventListener('input', function () {
    let time = parseFloat(transportSlider.value);
    ws.send(JSON.stringify({ Message: "Transport", Milliseconds: time }));
});

// https://stackoverflow.com/questions/1068834/object-comparison-in-javascript
// jfc javascript
function isSamePath(path1, path2) {
    return JSON.stringify(path1) === JSON.stringify(path2);
}
function isDifferentPath(path1, path2) {
    return JSON.stringify(path1) !== JSON.stringify(path2);
}

function updateUI() {
    var div = document.getElementById("buttons");
    div.innerHTML = '';

    triggers.forEach(element => {
        var button = document.createElement("button");
        button.innerHTML = element.Name;

        div.appendChild(button);

        button.addEventListener("click", event => {
            ws.send(JSON.stringify({
                Message: "Trigger",
                Path: element.Path
            }));
        });
    });

    div.appendChild(document.createElement("br"));
    intervals.forEach(element => {
        let label = document.createElement("span");
        label.innerHTML = element.Name + " speed: ";
        div.appendChild(label);

        var slider = document.createElement("input");
        slider.type = 'range';
        slider.min = -1;
        slider.max = 5;
        slider.value = element.speed;
        slider.step = 0.01;
        div.appendChild(slider);

        slider.oninput = function () {
            ws.send(JSON.stringify({
                Message: "IntervalSpeed",
                Path: element.Path,
                Speed: parseFloat(slider.value)
            }));
        };

        div.appendChild(document.createElement("br"));
    });

    controls.forEach(element => {
        element.Controls.forEach(ctrl => {
            let label = document.createElement("span");
            label.innerHTML = ctrl.Custom;
            div.appendChild(label);

            var slider = document.createElement("input");
            slider.type = 'range';
            slider.min = 0;
            slider.max = 1;
            slider.value = ctrl.Value.Float;
            slider.step = 0.01;
            div.appendChild(slider);

            slider.oninput = function () {
                ws.send(JSON.stringify({
                    Message: "ControlSurface",
                    Path: element.Path,
                    id: ctrl.id,
                    Value: { Float: parseFloat(slider.value) }
                }));
            };
        });
        div.appendChild(document.createElement("br"));
    });
}

let messageProcessor = {
    TriggerAdded: (obj) => {
        if (triggers.find(element => isSamePath(element.Path, obj.Path)) !== undefined)
            return;

        triggers.push(obj);
        updateUI();
    },
    TriggerRemoved: (obj) => {
        triggers = triggers.filter(element => isDifferentPath(element.Path, obj.Path));
        updateUI();
    },
    IntervalAdded: (obj) => {
        if (intervals.find(element => isSamePath(element.Path, obj.Path)) !== undefined)
            return;

        intervals.push(obj);
        if (intervals.length === 1) {
            // we added the first interval which is the root interval
            let transportSlider = document.getElementById("transport");
            transportSlider.max = obj.DefaultDuration;
        }

        updateUI();
    },
    IntervalRemoved: (obj) => {
        intervals = intervals.filter(element => isDifferentPath(element.Path, obj.Path));
        updateUI();
    },
    ControlSurfaceAdded: (obj) => {
        controls.push(obj);
        updateUI();
    },
    ControlSurfaceRemoved: (obj) => {
        controls = controls.filter(element => isDifferentPath(element.Path, obj.Path));
        updateUI();
    },
}

function connectToWS() {
    var endpoint = document.getElementById("endpoint").value;
    if (ws !== undefined) {
        ws.close()
    }

    ws = new WebSocket(endpoint);
    ws.onmessage = function (event) {
        var obj = JSON.parse(event.data);
        const handler = messageProcessor[obj.Message];
        if (handler !== undefined) {
            handler(obj);
        }
    }

    globalThis.play = () => {
        ws.send(JSON.stringify({ Message: "Play" }));
    }

    globalThis.pause = () => {
        ws.send(JSON.stringify({ Message: "Pause" }));
    }

    globalThis.stop = () => {
        ws.send(JSON.stringify({ Message: "Stop" }));
    }
}
