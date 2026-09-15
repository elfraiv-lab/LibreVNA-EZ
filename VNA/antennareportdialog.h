#ifndef ANTENNAREPORTDIALOG_H
#define ANTENNAREPORTDIALOG_H

#include "antennareport.h"

#include <QDialog>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QCheckBox>

// Collects name, notes and content options for the antenna PDF report.
// The file location is chosen by the caller after the dialog is accepted.
class AntennaReportDialog : public QDialog
{
    Q_OBJECT
public:
    explicit AntennaReportDialog(const QString &antennaName, QWidget *parent = nullptr);

    QString antennaName() const { return eName->text().trimmed(); }
    QString notes() const { return eNotes->toPlainText(); }
    AntennaReport::Options options() const;

private:
    QLineEdit *eName;
    QPlainTextEdit *eNotes;
    QCheckBox *cbPlots, *cbMarkers, *cbTouchstone;
};

#endif // ANTENNAREPORTDIALOG_H
