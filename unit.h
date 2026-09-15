#ifndef UNIT_H
#define UNIT_H

#include <QString>
#include <complex>

namespace Unit
{
    double FromString(QString string, QString unit = QString(), QString prefixes = " ");
    // prefixed need to be in ascending order (e.g. "m kMG" is okay, whjle "MkG" does not work)
    // precision: significant digits. maxDecimals >= 0 additionally caps the digits after the decimal point
    QString ToString(double value, QString unit = QString(), QString prefixes = " ", int precision = 6, int maxDecimals = -1);
    double SIPrefixToFactor(char prefix);
}

#endif // UNIT_H
