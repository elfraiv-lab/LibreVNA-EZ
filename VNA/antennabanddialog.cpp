#include "antennabanddialog.h"

#include "CustomWidgets/siunitedit.h"
#include "unit.h"
#include "cablelibrary.h"

#include <QFormLayout>
#include <QVBoxLayout>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QPushButton>
#include <cmath>

double AntennaBandDialog::Settings::sweepStart() const
{
    double bw = fHigh - fLow;
    double margin = bw > 0 ? bw * marginPercent / 100.0 : fLow * marginPercent / 100.0;
    return fLow - margin;
}

double AntennaBandDialog::Settings::sweepStop() const
{
    double bw = fHigh - fLow;
    double margin = bw > 0 ? bw * marginPercent / 100.0 : fHigh * marginPercent / 100.0;
    return fHigh + margin;
}

AntennaBandDialog::AntennaBandDialog(const Settings &initial, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Antenna band");
    setAttribute(Qt::WA_DeleteOnClose);

    auto layout = new QVBoxLayout(this);

    auto bandBox = new QGroupBox("Required band");
    auto form = new QFormLayout(bandBox);
    eLow = new SIUnitEdit("Hz", " kMG", 6);
    eLow->setMaxDecimals(3);
    eLow->setValue(initial.fLow);
    form->addRow("From:", eLow);
    eHigh = new SIUnitEdit("Hz", " kMG", 6);
    eHigh->setMaxDecimals(3);
    eHigh->setValue(initial.fHigh);
    form->addRow("To:", eHigh);
    eVSWR = new SIUnitEdit("", " ", 3);
    eVSWR->setValue(initial.vswr);
    eVSWR->setToolTip("Maximum VSWR inside the band");
    form->addRow("Max VSWR:", eVSWR);
    eMargin = new SIUnitEdit("%", " ", 3);
    eMargin->setValue(initial.marginPercent);
    eMargin->setToolTip("Extra span shown on both sides of the band, in percent of the bandwidth");
    form->addRow("Sweep margin:", eMargin);
    layout->addWidget(bandBox);

    auto cableBox = new QGroupBox("Cable between calibration plane and antenna");
    auto cableForm = new QFormLayout(cableBox);
    cbCable = new QComboBox;
    cbCable->addItem(CableLibrary::noneName());
    cbCable->addItems(CableLibrary::names());
    cbCable->addItem(CableLibrary::customName());
    cbCable->setCurrentText(initial.cableType.isEmpty() ? CableLibrary::noneName() : initial.cableType);
    cbCable->setToolTip("Delay and loss of this cable are removed with a port extension, so VSWR refers to the antenna connector");
    cableForm->addRow("Cable:", cbCable);
    eCableLength = new SIUnitEdit("m", " ", 4);
    eCableLength->setValue(initial.cableLength);
    cableForm->addRow("Length:", eCableLength);
    eCableVF = new SIUnitEdit("", " ", 3);
    eCableVF->setValue(initial.cableVF);
    cableForm->addRow("Velocity factor:", eCableVF);
    eCableLoss = new SIUnitEdit("dB/m", " ", 3);
    eCableLoss->setValue(initial.cableLoss);
    eCableLoss->setToolTip("One-way attenuation per meter at 1 GHz (scaled with sqrt(f))");
    cableForm->addRow("Loss @ 1 GHz:", eCableLoss);
    layout->addWidget(cableBox);

    auto actionBox = new QGroupBox("Apply");
    auto actions = new QVBoxLayout(actionBox);
    cbSweep = new QCheckBox("Set sweep to band with margin");
    cbSweep->setChecked(initial.setSweep);
    actions->addWidget(cbSweep);
    cbLimit = new QCheckBox("Limit line on VSWR graph (PASS/FAIL)");
    cbLimit->setChecked(initial.addLimitLine);
    actions->addWidget(cbLimit);
    cbMarker = new QCheckBox("Antenna band marker on S11");
    cbMarker->setChecked(initial.addMarker);
    actions->addWidget(cbMarker);
    layout->addWidget(actionBox);

    lSummary = new QLabel;
    lSummary->setWordWrap(true);
    layout->addWidget(lSummary);

    auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);

    auto revalidate = [=]() {
        // keep the band ordered and the VSWR meaningful
        if(eHigh->value() < eLow->value()) {
            eHigh->setValueQuiet(eLow->value());
        }
        if(eVSWR->value() < 1.01) {
            eVSWR->setValueQuiet(1.01);
        }
        if(eMargin->value() < 0) {
            eMargin->setValueQuiet(0);
        }
        updateSummary();
    };
    for(auto e : {eLow, eHigh, eVSWR, eMargin}) {
        connect(e, &SIUnitEdit::valueChanged, this, revalidate);
    }
    connect(cbSweep, &QCheckBox::toggled, this, &AntennaBandDialog::updateSummary);
    connect(cbCable, &QComboBox::currentTextChanged, this, [=](const QString &) {
        updateCableFields();
        updateSummary();
    });
    connect(eCableLength, &SIUnitEdit::valueChanged, this, [=]() {
        if(eCableLength->value() < 0) {
            eCableLength->setValueQuiet(0);
        }
        updateSummary();
    });
    updateCableFields();
    updateSummary();
}

void AntennaBandDialog::updateCableFields()
{
    auto name = cbCable->currentText();
    bool none = (name == CableLibrary::noneName());
    bool custom = (name == CableLibrary::customName());
    if(auto c = CableLibrary::find(name)) {
        eCableVF->setValueQuiet(c->vf);
        eCableLoss->setValueQuiet(c->lossPerMeter(1e9));
    }
    eCableLength->setEnabled(!none);
    eCableVF->setEnabled(custom);
    eCableLoss->setEnabled(custom);
}

AntennaBandDialog::Settings AntennaBandDialog::settings() const
{
    Settings s;
    s.fLow = eLow->value();
    s.fHigh = eHigh->value();
    s.vswr = eVSWR->value();
    s.marginPercent = eMargin->value();
    s.setSweep = cbSweep->isChecked();
    s.addLimitLine = cbLimit->isChecked();
    s.addMarker = cbMarker->isChecked();
    s.cableType = cbCable->currentText();
    s.cableLength = eCableLength->value();
    s.cableVF = eCableVF->value();
    s.cableLoss = eCableLoss->value();
    return s;
}

void AntennaBandDialog::updateSummary()
{
    auto s = settings();
    QString text;
    if(s.setSweep) {
        text = "Sweep: " + Unit::ToString(s.sweepStart(), "Hz", " kMG", 6, 3)
                + " – " + Unit::ToString(s.sweepStop(), "Hz", " kMG", 6, 3);
    } else {
        text = "Sweep unchanged";
    }
    // the VSWR limit expressed as return loss, handy when only a dB graph is shown
    double gamma = (s.vswr - 1.0) / (s.vswr + 1.0);
    text += ", |S11| ≤ " + QString::number(20.0 * log10(gamma), 'f', 1) + " dB";
    if(s.cableType != CableLibrary::noneName() && s.cableLength > 0) {
        double fc = (s.fLow + s.fHigh) / 2;
        double lossPerMeter = s.cableLoss * sqrt(fc / 1e9);
        if(auto c = CableLibrary::find(s.cableType)) {
            lossPerMeter = c->lossPerMeter(fc);
        }
        text += QString(". Cable: %1 dB one way at band center, removed by port extension")
                .arg(lossPerMeter * s.cableLength, 0, 'f', 2);
    }
    lSummary->setText(text);
}
