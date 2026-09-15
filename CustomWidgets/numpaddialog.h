#ifndef NUMPADDIALOG_H
#define NUMPADDIALOG_H

#include <QDialog>
#include <QLineEdit>

// Touch-friendly numeric keypad with SI prefix buttons.
// Returns the entered value (already multiplied by the chosen prefix).
class NumPadDialog : public QDialog
{
    Q_OBJECT
public:
    // unit: e.g. "Hz", prefixes: allowed SI prefix chars (as in SIUnitEdit),
    // initial: current value shown as hint, precision: digits for display
    NumPadDialog(QString unit, QString prefixes, double initial, int precision, QWidget *parent = nullptr, int maxDecimals = -1);

    double value() const { return _value; }

    // Convenience: opens the dialog under/near anchor, returns true if accepted
    static bool getValue(QWidget *anchor, QString unit, QString prefixes, double initial, int precision, double &result, int maxDecimals = -1);

private:
    void append(const QString &s);
    void backspace();
    void finish(double factor);
    void positionNear(QWidget *anchor);

    QLineEdit *display;
    QString unit, prefixes;
    double _value;
    int precision;
    int maxDecimals;
    bool replaceOnInput;
};

#endif // NUMPADDIALOG_H
