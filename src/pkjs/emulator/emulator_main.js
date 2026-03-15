var location = require("../location");
var reminders = require("../reminders");
var emulatorSession = require("./emulator_session");
var config = require("../config");

function main() {
    location.update();
    Pebble.addEventListener('appmessage', handleAppMessage);
}


function handleAppMessage(e) {
    console.log("Inbound app message!");
    console.log(JSON.stringify(e));
    var data = e.payload;
    if (data.PROMPT) {
        console.log("Starting a new Session...");
        var s = new emulatorSession.Session(data.PROMPT, data.THREAD_ID);
        s.run();
        return;
    }

    if (reminders.handleReminderMessage(data)) {
        return;
    }

    if ('LOCATION_ENABLED' in data) {
        config.setSetting("LOCATION_ENABLED", !!data.LOCATION_ENABLED);
        console.log("Location enabled: " + config.isLocationEnabled());
        // We need to confirm that we received this for the watch to proceed.
        Pebble.sendAppMessage({
            LOCATION_ENABLED: data.LOCATION_ENABLED,
        });
    }
}

exports.main = main;
