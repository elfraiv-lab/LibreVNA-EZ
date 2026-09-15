#ifndef CABLELIBRARY_H
#define CABLELIBRARY_H

#include <QString>
#include <QStringList>
#include <vector>

// Small library of coaxial cable types used by the antenna band and cable
// impedance quick setups: velocity factor and attenuation model.
class CableLibrary
{
public:
    class Cable {
    public:
        QString name;
        double vf;              // velocity factor
        double z0;              // nominal impedance, Ω
        // attenuation (dB/100 m) = a * sqrt(f/MHz) + b * (f/MHz); if both are 0, lossAt1GHz * sqrt(f/1GHz) is used
        double a;
        double b;
        double lossAt1GHz;      // dB/m, one way

        // one-way attenuation in dB per meter at frequency f (Hz)
        double lossPerMeter(double f) const;
    };

    static const std::vector<Cable>& cables();
    static QStringList names();
    // nullptr for unknown names (e.g. "Custom" / "None")
    static const Cable* find(const QString &name);

    static QString noneName() { return "None"; }
    static QString customName() { return "Custom"; }
};

#endif // CABLELIBRARY_H
