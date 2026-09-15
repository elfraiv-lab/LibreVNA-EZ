#ifndef UNDOHISTORY_H
#define UNDOHISTORY_H

#include "json.hpp"

#include <QObject>
#include <QTimer>
#include <functional>
#include <vector>
#include <string>

// Snapshot based undo/redo for a mode setup (sweep, traces, graphs, markers).
//
// The setup is captured as JSON on a timer; whenever the normalized snapshot differs
// from the last committed one, the previous snapshot becomes an undo entry. Undo/redo
// restore a snapshot through the mode's fromJSON. Volatile fields that change on their
// own (data hashes, auto-updated marker positions, splitter sizes) are ignored when
// comparing so that sweeps and window resizes do not create history entries.
class UndoHistory : public QObject
{
    Q_OBJECT
public:
    using Capture = std::function<nlohmann::json()>;
    using Restore = std::function<void(const nlohmann::json&)>;

    UndoHistory(Capture capture, Restore restore, QObject *parent = nullptr);

    bool canUndo() const { return !undoStack.empty(); }
    bool canRedo() const { return !redoStack.empty(); }

public slots:
    // compares the current setup with the last committed snapshot, records a change
    void checkpoint();
    void undo();
    void redo();
    // forgets the history and takes the current setup as the base state
    void reset();

signals:
    void availabilityChanged(bool canUndo, bool canRedo);

private:
    static nlohmann::json normalized(const nlohmann::json &j);
    static std::string key(const nlohmann::json &j) { return normalized(j).dump(); }
    void apply(const nlohmann::json &snapshot);
    void emitAvailability();

    static constexpr size_t maxEntries = 50;
    static constexpr int pollIntervalMs = 500;

    Capture capture;
    Restore restore;
    std::vector<nlohmann::json> undoStack;
    std::vector<nlohmann::json> redoStack;
    nlohmann::json current;
    std::string currentKey;
    bool restoring = false;
    bool primed = false;   // first checkpoint after construction only sets the base state
    QTimer timer;
};

#endif // UNDOHISTORY_H
