#ifndef TRACEMARKER_H
#define TRACEMARKER_H

#include "../trace.h"
#include "CustomWidgets/siunitedit.h"
#include "savable.h"

#include <QPixmap>
#include <QObject>
#include <QMenu>
#include <QComboBox>

class MarkerModel;
class MarkerGroup;

class Marker : public QObject, public Savable
{
    Q_OBJECT
public:
    Marker(MarkerModel *model, int number = 1, Marker *parent = nullptr, QString descr = QString());
    ~Marker();
    void assignTrace(Trace *t);
    Trace* trace();

    enum class Format {
        dB,
        dBm,
        dBuV,
        dBAngle,
        RealImag,
        Impedance,
        VSWR,
        SeriesR,
        Capacitance,
        Inductance,
        QualityFactor,
        GroupDelay,
        // Peak table
        NumberOfPeaks,
        // Noise marker parameters
        Noise,
        PhaseNoise,
        // Filter parameters
        CenterBandwidth,
        Cutoff,
        InsertionLoss,
        // TOI parameters
        TOI,                    // third order intercept point
        AvgTone,                // average level of tone
        AvgModulationProduct,   // average level of modulation products
        // compression parameters
        P1dB,                   // power level at 1dB compression
        Flatness,
        maxDeltaPos,
        maxDeltaNeg,
        // antenna band parameters (VSWR threshold)
        BandEdges,
        Bandwidth,
        // band statistics (min/max/mean over the restricted range, ripple check)
        StatVSWR,
        StatMagnitude,
        Ripple,
        // cable impedance (TDR step response)
        CableImpedance,
        CableLength,
        CableDelay,
        // cable loss (one port, far end open or shorted)
        CableLoss,
        CableLossBand,
        CableLossPerLength,
        // keep last at end
        Last,
    };

    static QString formatToString(Format f);
    static Format formatFromString(QString s);
    static std::vector<Format> formats();
    std::vector<Format> applicableFormats();
    std::vector<Format> defaultActiveFormats();

    QString readableData(Format format = Format::Last);
    QString readablePosition();
    QString readableSettings();
    QString tooltipSettings();
    QString readableType();
    QString domainToUnit();
    static QString domainToUnit(Trace::DataType domain);

    double getPosition() const;
    std::complex<double> getData() const;
    bool isMovable();
    bool isEditable();
    Trace::DataType getDomain();

    class Line {
    public:
        TraceMath::Data p1, p2;
    };
    std::vector<Line> getLines();

    QPixmap& getSymbol();

    unsigned long getCreationTimestamp() const;

    int getNumber() const;
    void setNumber(int value);

    bool editingFrequency;
    Trace *getTrace() const;

    enum class Type {
        Manual,
        Maximum,
        Minimum,
        Delta,
        PeakTable,
        NegativePeakTable,
        Lowpass,
        Highpass,
        Bandpass,
        TOI,
        PhaseNoise,
        P1dB,
        Flatness,
        AntennaBand,
        Statistics,
        CableImpedance,
        CableLoss,
        // keep last at end
        Last,
    };
    Type getType() const;
    void setType(Type t);
    // limit automatic marker types (extrema, antenna band, ...) to a frequency range
    void setRestriction(bool enabled, double min, double max);
    // threshold for the AntennaBand type (VSWR, must be > 1)
    void setVSWRThreshold(double threshold);
    double getVSWRThreshold() const { return vswrThreshold; }
    // Statistics type: min/max/mean of |S| (and VSWR for reflection) over the restricted
    // range, marker sits at the worst point; ripple = peak-to-peak of the detrended |S| in dB
    struct BandStats {
        bool valid = false;
        double fLow = 0, fHigh = 0;
        double magMin = 0, magMax = 0, magMean = 0;   // dB
        double vswrMin = 0, vswrMax = 0, vswrMean = 0; // reflection traces only
        double ripplePP = 0;                            // dB
        // cable loss (one way = |S11| dB / 2 with the ripple smoothed out), CableLoss type only
        double lossMin = 0, lossMax = 0, lossMean = 0;  // dB
        bool lossSuspicious = false;                    // |S11| above 0 dB somewhere: no calibration?
    };
    const BandStats &getBandStats() const { return stats; }
    // ripple limit in dB peak-to-peak, 0 = no check
    void setRippleLimit(double dB);
    double getRippleLimit() const { return rippleLimit; }
    bool hasRippleLimit() const { return rippleLimit > 0; }
    bool isRipplePass() const { return stats.valid && stats.ripplePP <= rippleLimit; }
    // CableImpedance type: tolerance of the flat section search (Ω) and the known
    // cable length (m, 0 = unknown -> length is derived from the trace velocity factor)
    void setZTolerance(double tolerance);
    void setKnownLength(double length);
    // datasheet velocity factor of the cable (0 = unknown), shown next to the measured one
    void setNominalVF(double vf);
    // CableLoss type: cable library entry used for the datasheet comparison ("" = none).
    // The length of the cable under test is taken from setKnownLength() (0 = unknown, no dB/m)
    void setCableType(const QString &name);
    QString getCableType() const { return cableTypeName; }
    // forget a manually adjusted section, search automatically again
    void resetSection();
    double getCableImpedance() const { return cableMeanZ; }
    bool isCableValid() const { return cableValid; }
    QWidget *getTraceEditor(QAbstractItemDelegate *delegate = nullptr);
    void updateTraceFromEditor(QWidget *w);
    QWidget *getTypeEditor(QAbstractItemDelegate *delegate = nullptr);
    void updateTypeFromEditor(QWidget *w);
    SIUnitEdit* getSettingsEditor();
    QWidget *getRestrictEditor();
    void adjustSettings(double value);
    bool isVisible();
    void setVisible(bool visible);

    QMenu *getContextMenu();

    // Updates marker position and data on automatic markers. Should be called whenever the tracedata is complete
    void update();
    Marker *getParent() const;
    const std::vector<Marker *>& getHelperMarkers() const;
    Marker *helperMarker(unsigned int i);
    bool canUseAsDelta(Marker *m);
    void assignDeltaMarker(Marker *m);
    QString getSuffix() const;

    virtual nlohmann::json toJSON() override;
    virtual void fromJSON(nlohmann::json j) override;
    // Markers are referenced by pointers throughout this project (e.g. when added to a trace)
    // When saving the current marker configuration, the pointer is not useful (e.g. for determining
    // the associated delta marker. Instead a marker hash is saved to identify the correct marker.
    // The hash should be influenced by every setting the marker can have. It should not depend on
    // the marker data.
    unsigned int toHash();


    std::set<Format> getGraphDisplayFormats() const;

    MarkerGroup *getGroup() const;
    void setGroup(MarkerGroup *value);

public slots:
    void setPosition(double freq);
    void setToMiddleOfTrace();
    void updateContextmenu();
signals:
    void positionChanged(double pos);
    void deleted(Marker *m);
    void dataChanged(Marker *m);
    void visibilityChanged(Marker *m);
    void symbolChanged(Marker *m);
    void typeChanged(Marker *m);
    void assignedDeltaChanged(Marker *m);
    void traceChanged(Marker *m);
    void beginRemoveHelperMarkers(Marker *m);
    void endRemoveHelperMarkers(Marker *m);
    void dataFormatChanged(Marker *m);

private slots:
    void parentTraceDeleted(Trace *t);
    void traceDataChanged(unsigned int begin, unsigned int end);
    void updateSymbol();
    void checkDeltaMarker();
    void deltaDeleted();
    void traceTypeChanged();
signals:
    void rawDataChanged();
    void domainChanged(Marker *m);
private:
    std::set<Type> getSupportedTypes();
    static QString typeToString(Type t) {
        switch(t) {
        case Type::Manual: return "Manual";
        case Type::Maximum: return "Maximum";
        case Type::Minimum: return "Minimum";
        case Type::Delta: return "Delta";
        case Type::PeakTable: return "Peak Table";
        case Type::NegativePeakTable: return "Negative Peak Table";
        case Type::Lowpass: return "Lowpass";
        case Type::Highpass: return "Highpass";
        case Type::Bandpass: return "Bandpass";
        case Type::TOI: return "TOI/IP3";
        case Type::PhaseNoise: return "Phase noise";
        case Type::P1dB: return "1dB compression";
        case Type::Flatness: return "Flatness";
        case Type::AntennaBand: return "Antenna band (VSWR)";
        case Type::Statistics: return "Band statistics";
        case Type::CableImpedance: return "Cable impedance (TDR)";
        case Type::CableLoss: return "Cable loss (open/short)";
        default: return QString();
        }
    }
    void constrainPosition();
    void constrainFormat();
    Marker *bestDeltaCandidate();
    void deleteHelperMarkers();
    double toDecibel();
    bool isDisplayedMarker();

    void setTableFormat(Format f);

    MarkerModel *model;
    Trace *parentTrace;
    unsigned long creationTimestamp;
    double position;
    double minPosition;
    double maxPosition;
    bool restrictPosition;
    int number;
    bool visible;
    // Frequency domain: S parameter
    // Time domain: impulse response
    std::complex<double> data;
    QPixmap symbol;
    Type type;
    QString suffix;
    QString description;

    QMenu contextmenu;

    Marker *delta;
    std::vector<Marker*> helperMarkers;
    Marker *parent;

    // additional lines the marker wants to show on the graphs (the graphs are responsible for drawing the lines)
    std::vector<Line> lines;

    // settings for the different marker types
    double cutoffAmplitude;
    double peakThreshold;
    double offset;
    // antenna band: VSWR threshold defining the usable band around the resonance
    double vswrThreshold;
    bool bandFound;         // resonance VSWR is below the threshold
    bool bandLowClipped;    // lower edge limited by trace/restriction range, not by the threshold
    bool bandHighClipped;   // upper edge limited by trace/restriction range, not by the threshold
    // band statistics
    BandStats stats;
    double rippleLimit = 0.0;
    // cable impedance (TDR)
    double zTolerance;      // Ω, flat section = impedance stays within this tolerance of the running mean
    bool sectionManual;     // helper markers were moved by the user, keep them
    bool sectionUpdating;   // helper positions are being set programmatically
    double knownLength;     // m, 0 = unknown
    double nominalVF;       // datasheet VF, 0 = unknown
    QString cableTypeName;  // cable library entry, empty = none (CableLoss)
    bool cableValid;
    bool cableClipped;      // flat section reaches the end of the data/restriction range
    int cableEndKind;       // step behind the section: +1 open (rho > 0.5), -1 short (rho < -0.5), 0 none/unknown
    double cableMeanZ, cableMinZ, cableMaxZ;
    double cableStartTime, cableEndTime;  // untrimmed edges of the flat section (round trip time)

    // non-uniformity
    double maxDeltaNeg;
    double maxDeltaPos;

    Format formatTable;
    std::set<Format> formatGraph;

    MarkerGroup *group;
};

#endif // TRACEMARKER_H
