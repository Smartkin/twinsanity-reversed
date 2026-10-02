"""Keeps the PCSX2 windows a run opens out of the way on KDE Plasma (6): a KWin script loaded for the run sends every PCSX2 window
that appears to one monitor, keeps it below other windows and gives the focus back to the window that had it (for a few seconds
after the window appears: clicked later, it keeps the focus). It's unloaded when the run ends, nothing goes into KWin's
configuration. Without KWin (qdbus6) the windows go wherever the desktop puts them."""
import contextlib
import json
import os
import shutil
import subprocess
import tempfile

NAME = "twinsanity-decomp-pcsx2"
# The monitor (KWin's output name, kscreen-doctor -o lists them); PCSX2_MONITOR overrides it, empty leaves the windows alone
DEFAULT_MONITOR = "HDMI-A-1"

SCRIPT = """
const monitor = @MONITOR@;
// The user's own windows (a PCSX2 of theirs included) are left alone
const before = workspace.windowList();
const opened = [];
let previous = workspace.activeWindow;

function isPcsx2(window) {
    const names = [window.resourceClass, window.resourceName, window.desktopFileName];
    for (let i = 0; i < names.length; i++) {
        if (String(names[i] || "").toLowerCase().indexOf("pcsx2") >= 0) {
            return true;
        }
    }
    return false;
}

function isTheRuns(window) {
    return isPcsx2(window) && before.indexOf(window) < 0;
}

function place(window) {
    const screens = workspace.screens;
    for (let i = 0; i < screens.length; i++) {
        if (screens[i].name === monitor && window.output !== screens[i]) {
            workspace.sendClientToScreen(window, screens[i]);
        }
    }
    window.keepBelow = true;
}

// When the run's window appeared (KWin may activate it before it announces it)
function openedAt(window) {
    for (let i = 0; i < opened.length; i++) {
        if (opened[i].window === window) {
            return opened[i].since;
        }
    }
    opened.push({window: window, since: Date.now()});
    place(window);
    window.outputChanged.connect(function () {
        place(window);
    });
    print(@NAME@ + ": " + window.resourceClass + " on " + (window.output ? window.output.name : "?"));
    return Date.now();
}

function giveFocusBack() {
    if (previous !== null) {
        workspace.activeWindow = previous;
    }
}

workspace.windowAdded.connect(function (window) {
    if (!isTheRuns(window)) {
        return;
    }
    openedAt(window);
    if (workspace.activeWindow === window) {
        giveFocusBack();
    }
});

workspace.windowActivated.connect(function (window) {
    if (window === null) {
        return;
    }
    if (!isTheRuns(window)) {
        previous = window;
    } else if (Date.now() - openedAt(window) < 5000) {
        giveFocusBack();
    }
});

workspace.windowRemoved.connect(function (window) {
    if (window === previous) {
        previous = null;
    }
});
"""


def _kwin(*arguments):
    return subprocess.run(["qdbus6", "org.kde.KWin", *arguments], capture_output=True, text=True, timeout=10)


@contextlib.contextmanager
def windows_on(monitor):
    if not monitor or shutil.which("qdbus6") is None:
        yield
        return

    with tempfile.TemporaryDirectory(prefix="decomp-kwin-") as folder:
        path = os.path.join(folder, "place.js")
        with open(path, "w") as file:
            file.write(SCRIPT.replace("@MONITOR@", json.dumps(monitor)).replace("@NAME@", json.dumps(NAME)))

        loaded = False
        try:
            # A run that was killed may have left its script
            _kwin("/Scripting", "org.kde.kwin.Scripting.unloadScript", NAME)
            script = _kwin("/Scripting", "org.kde.kwin.Scripting.loadScript", path, NAME).stdout.strip()
            if script.isdigit():
                loaded = _kwin(f"/Scripting/Script{script}", "org.kde.kwin.Script.run").returncode == 0
        except (OSError, subprocess.SubprocessError):
            pass

        try:
            yield
        finally:
            if loaded:
                with contextlib.suppress(OSError, subprocess.SubprocessError):
                    _kwin("/Scripting", "org.kde.kwin.Scripting.unloadScript", NAME)


def monitor_from(argument):
    return argument if argument is not None else os.environ.get("PCSX2_MONITOR", DEFAULT_MONITOR)
