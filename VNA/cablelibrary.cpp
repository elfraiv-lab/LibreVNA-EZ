#include "cablelibrary.h"

#include <cmath>

double CableLibrary::Cable::lossPerMeter(double f) const
{
    if(f <= 0) {
        return 0.0;
    }
    if(a != 0.0 || b != 0.0) {
        double fMHz = f / 1e6;
        return (a * sqrt(fMHz) + b * fMHz) / 100.0;
    }
    return lossAt1GHz * sqrt(f / 1e9);
}

const std::vector<CableLibrary::Cable>& CableLibrary::cables()
{
    // RG-58 A/U: Radiolab datasheet (FPE dielectric, 83.94 pF/m -> VF 0.795), attenuation table
    // 150 MHz 13.8, 450 MHz 25.2, 900 MHz 36.6, 1800 MHz 53.8, 2450 MHz 64.7 dB/100 m fitted with a*sqrt(f)+b*f.
    // The other entries are typical catalog values (sqrt(f) model, loss at 1 GHz).
    static const std::vector<Cable> list = {
        {"RG-58 A/U (FPE)",     0.795, 50.0, 1.086, 0.00447, 0.0},
        {"RG-58 C/U (solid PE)", 0.66, 50.0, 0.0,   0.0,     0.50},
        {"RG-316",              0.695, 50.0, 0.0,   0.0,     0.98},
        {"RG-174",              0.66,  50.0, 0.0,   0.0,     0.89},
        {"LMR-240",             0.84,  50.0, 0.0,   0.0,     0.25},
    };
    return list;
}

QStringList CableLibrary::names()
{
    QStringList ret;
    for(auto &c : cables()) {
        ret.push_back(c.name);
    }
    return ret;
}

const CableLibrary::Cable *CableLibrary::find(const QString &name)
{
    for(auto &c : cables()) {
        if(c.name == name) {
            return &c;
        }
    }
    return nullptr;
}
