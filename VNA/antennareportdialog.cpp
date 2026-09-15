#include "antennareportdialog.h"

#include <QVBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QLabel>

AntennaReportDialog::AntennaReportDialog(const QString &antennaName, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Antenna report");
    setAttribute(Qt::WA_DeleteOnClose);

    auto layout = new QVBoxLayout(this);

    auto form = new QFormLayout;
    eName = new QLineEdit(antennaName);
    eName->setPlaceholderText("e.g. 2.4 GHz dipole, sample #3");
    form->addRow("Antenna:", eName);
    eNotes = new QPlainTextEdit;
    eNotes->setPlaceholderText("Cable, position, tuning state, operator...");
    eNotes->setMaximumHeight(90);
    form->addRow("Notes:", eNotes);
    layout->addLayout(form);

    auto box = new QGroupBox("Content");
    auto content = new QVBoxLayout(box);
    cbPlots = new QCheckBox("Graphs (rendered on white background)");
    cbPlots->setChecked(true);
    content->addWidget(cbPlots);
    cbMarkers = new QCheckBox("Marker tables (antenna band, other markers)");
    cbMarkers->setChecked(true);
    content->addWidget(cbMarkers);
    cbTouchstone = new QCheckBox("Save S11 as Touchstone (.s1p) next to the PDF");
    cbTouchstone->setChecked(true);
    content->addWidget(cbTouchstone);
    layout->addWidget(box);

    auto hint = new QLabel("The report always contains the setup, the resonance/worst VSWR summary "
                           "and the limit line results.");
    hint->setWordWrap(true);
    layout->addWidget(hint);

    auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    buttons->button(QDialogButtonBox::Ok)->setText("Save PDF...");
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
}

AntennaReport::Options AntennaReportDialog::options() const
{
    AntennaReport::Options o;
    o.includePlots = cbPlots->isChecked();
    o.includeMarkers = cbMarkers->isChecked();
    o.saveTouchstone = cbTouchstone->isChecked();
    return o;
}
