#include "undohistory.h"

#include <QDebug>

UndoHistory::UndoHistory(Capture capture, Restore restore, QObject *parent)
    : QObject(parent),
      capture(capture),
      restore(restore)
{
    timer.setInterval(pollIntervalMs);
    connect(&timer, &QTimer::timeout, this, &UndoHistory::checkpoint);
    // let the mode finish its construction / initial setup load before taking the base state
    QTimer::singleShot(1500, this, [this]() { timer.start(); });
}

nlohmann::json UndoHistory::normalized(const nlohmann::json &j)
{
    static const char *volatileKeys[] = {
        "hash",             // trace hash includes the live data
        "sizes",            // splitter sizes follow the window size
        "position",         // marker position: auto markers move with every sweep
        "section_start",    // cable impedance auto section
        "section_end",
        "single",           // run/stop state
    };
    if(j.is_object()) {
        nlohmann::json out = nlohmann::json::object();
        for(auto it = j.begin(); it != j.end(); ++it) {
            bool skip = false;
            for(auto k : volatileKeys) {
                if(it.key() == k) {
                    skip = true;
                    break;
                }
            }
            if(!skip) {
                out[it.key()] = normalized(it.value());
            }
        }
        return out;
    } else if(j.is_array()) {
        nlohmann::json out = nlohmann::json::array();
        for(auto &v : j) {
            out.push_back(normalized(v));
        }
        return out;
    }
    return j;
}

void UndoHistory::checkpoint()
{
    if(restoring) {
        return;
    }
    nlohmann::json now;
    try {
        now = capture();
    } catch (...) {
        return;
    }
    auto k = key(now);
    if(!primed) {
        current = now;
        currentKey = k;
        primed = true;
        return;
    }
    if(k == currentKey) {
        // keep the most recent full snapshot (volatile fields included)
        current = now;
        return;
    }
    undoStack.push_back(current);
    if(undoStack.size() > maxEntries) {
        undoStack.erase(undoStack.begin());
    }
    redoStack.clear();
    current = now;
    currentKey = k;
    emitAvailability();
}

void UndoHistory::apply(const nlohmann::json &snapshot)
{
    restoring = true;
    try {
        restore(snapshot);
    } catch (const std::exception &e) {
        qWarning() << "Undo: failed to restore snapshot:" << e.what();
    }
    current = snapshot;
    currentKey = key(snapshot);
    // the restored setup may settle asynchronously (device reconfiguration, graph updates)
    QTimer::singleShot(pollIntervalMs, this, [this]() {
        restoring = false;
        try {
            current = capture();
            currentKey = key(current);
        } catch (...) {
        }
    });
}

void UndoHistory::undo()
{
    if(undoStack.empty()) {
        return;
    }
    // make sure pending edits are recorded first so redo brings them back
    checkpoint();
    auto snapshot = undoStack.back();
    undoStack.pop_back();
    redoStack.push_back(current);
    apply(snapshot);
    emitAvailability();
}

void UndoHistory::redo()
{
    if(redoStack.empty()) {
        return;
    }
    auto snapshot = redoStack.back();
    redoStack.pop_back();
    undoStack.push_back(current);
    apply(snapshot);
    emitAvailability();
}

void UndoHistory::reset()
{
    undoStack.clear();
    redoStack.clear();
    primed = false;
    checkpoint();
    emitAvailability();
}

void UndoHistory::emitAvailability()
{
    emit availabilityChanged(canUndo(), canRedo());
}
