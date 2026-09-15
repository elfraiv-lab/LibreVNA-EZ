#ifndef CABLEIMPEDANCEDIALOG_H
#define CABLEIMPEDANCEDIALOG_H

#include <QDialog>
#include <QRadioButton>
#include <QCheckBox>
#include <QLabel>
#include <QComboBox>

class SIUnitEdit;

// Quick setup for measuring the characteristic impedance of a cable section
// with TDR (step response of S11). Either the cable length is known and the
// velocity factor is derived from the measured delay, or the velocity factor
// is known and the length is derived.
class CableImpedanceDialog : public QDialog
{
    Q_OBJECT
public:
    class Settings {
    public:
        QString cableType;      // library entry or "Custom"
        double nominalVF;       // datasheet VF of the selected cable, 0 for custom
        bool knownLength;       // true: length given, compute VF; false: VF given, compute length
        double length;          // cable length in m (used when knownLength)
        double velocityFactor;  // 0..1 (used when !knownLength)
        double zTolerance;      // Ω, flat section search tolerance
        bool setSweep;          // configure a TDR-friendly sweep
        bool createTrace;       // create S11 trace with TDR step response + impedance/distance graph
        bool addMarker;         // add/update the cable impedance marker
    };

    explicit CableImpedanceDialog(const Settings &initial, QWidget *parent = nullptr);

    Settings settings() const;

private:
    void updateEnabled();

    QComboBox *cbCable;
    QRadioButton *rbLength, *rbVF;
    SIUnitEdit *eLength, *eVF, *eTolerance;
    QCheckBox *cbSweep, *cbTrace, *cbMarker;
    QLabel *lHint;
};

#endif // CABLEIMPEDANCEDIALOG_H
