#ifndef CABLELOSSDIALOG_H
#define CABLELOSSDIALOG_H

#include <QDialog>
#include <QCheckBox>
#include <QLabel>
#include <QComboBox>

class SIUnitEdit;

// Quick setup for the one port cable loss measurement: the far end of the cable
// is left open (or shorted), so the signal passes the cable twice and the one way
// loss is -|S11| dB / 2. The dialog only collects the values, the VNA applies them
// (sweep, loss trace, cable loss marker).
class CableLossDialog : public QDialog
{
    Q_OBJECT
public:
    class Settings {
    public:
        QString cableType;      // library entry or "Custom", used for the datasheet comparison
        double length;          // m, 0 = unknown (no dB/m)
        double fLow;            // start of the measured range (Hz)
        double fHigh;           // stop of the measured range (Hz)
        bool setSweep;          // set the sweep to fLow..fHigh
        bool createTrace;       // create a trace showing the one way loss in dB
        bool addMarker;         // add/update the cable loss marker
    };

    explicit CableLossDialog(const Settings &initial, QWidget *parent = nullptr);

    Settings settings() const;

private:
    void updateSummary();

    QComboBox *cbCable;
    SIUnitEdit *eLength, *eLow, *eHigh;
    QCheckBox *cbSweep, *cbTrace, *cbMarker;
    QLabel *lSummary;
};

#endif // CABLELOSSDIALOG_H
