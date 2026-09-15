#ifndef ANTENNABANDDIALOG_H
#define ANTENNABANDDIALOG_H

#include <QDialog>
#include <QCheckBox>
#include <QLabel>
#include <QComboBox>

class SIUnitEdit;

// Quick setup for an antenna measurement: the frequency band the antenna
// should cover and the maximum acceptable VSWR. The dialog only collects the
// values, the VNA applies them (sweep, limit line, antenna band marker).
class AntennaBandDialog : public QDialog
{
    Q_OBJECT
public:
    class Settings {
    public:
        double fLow;            // lower edge of the required band (Hz)
        double fHigh;           // upper edge of the required band (Hz)
        double vswr;            // maximum VSWR inside the band
        double marginPercent;   // extra span on both sides, in percent of the bandwidth
        bool setSweep;          // adjust the sweep to band +/- margin
        bool addLimitLine;      // add a PASS/FAIL limit line to a VSWR (or magnitude) graph
        bool addMarker;         // add/update an "Antenna band (VSWR)" marker
        // cable between calibration plane and antenna, compensated with a port extension
        QString cableType;      // "None", library entry or "Custom"
        double cableLength;     // m
        double cableVF;         // custom cable
        double cableLoss;       // custom cable, dB/m at 1 GHz
        double sweepStart() const;
        double sweepStop() const;
    };

    explicit AntennaBandDialog(const Settings &initial, QWidget *parent = nullptr);

    Settings settings() const;

private:
    void updateSummary();

    void updateCableFields();

    SIUnitEdit *eLow, *eHigh, *eVSWR, *eMargin;
    QComboBox *cbCable;
    SIUnitEdit *eCableLength, *eCableVF, *eCableLoss;
    QCheckBox *cbSweep, *cbLimit, *cbMarker;
    QLabel *lSummary;
};

#endif // ANTENNABANDDIALOG_H
