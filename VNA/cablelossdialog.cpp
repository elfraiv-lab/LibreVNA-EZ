#include "cablelossdialog.h"

#include "CustomWidgets/siunitedit.h"
#include "unit.h"
#include "cablelibrary.h"
#include "preferences.h"

#include <QVBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QDialogButtonBox>
#include <QPushButton>

CableLossDialog::CableLossDialog(const Settings &initial, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Cable loss");
    setAttribute(Qt::WA_DeleteOnClose);

    auto layout = new QVBoxLayout(this);

    auto cableBox = new QGroupBox("Cable under test");
    auto cableForm = new QFormLayout(cableBox);
    cbCable = new QComboBox;
    cbCable->addItems(CableLibrary::names());
    cbCable->addItem(CableLibrary::customName());
    cbCable->setCurrentText(initial.cableType.isEmpty() ? CableLibrary::customName() : initial.cableType);
    cbCable->setToolTip("Cable type from the library: the measured loss is compared to the datasheet value");
    cableForm->addRow("Cable:", cbCable);
    eLength = new SIUnitEdit("m", "m k", 4);
    eLength->setValue(initial.length);
    eLength->setToolTip("Physical length of the cable (0 = unknown, then only the total loss is shown)");
    cableForm->addRow("Length:", eLength);
    layout->addWidget(cableBox);

    auto measBox = new QGroupBox("Frequency range");
    auto measForm = new QFormLayout(measBox);
    eLow = new SIUnitEdit("Hz", " kMG", 6);
    eLow->setMaxDecimals(3);
    eLow->setValue(initial.fLow);
    measForm->addRow("From:", eLow);
    eHigh = new SIUnitEdit("Hz", " kMG", 6);
    eHigh->setMaxDecimals(3);
    eHigh->setValue(initial.fHigh);
    measForm->addRow("To:", eHigh);
    layout->addWidget(measBox);

    auto applyBox = new QGroupBox("Apply");
    auto applyLayout = new QVBoxLayout(applyBox);
    cbSweep = new QCheckBox("Set sweep to this frequency range");
    cbSweep->setChecked(initial.setSweep);
    applyLayout->addWidget(cbSweep);
    cbTrace = new QCheckBox("Create \"Cable loss\" trace (one way loss in dB) in a new graph");
    cbTrace->setChecked(initial.createTrace);
    applyLayout->addWidget(cbTrace);
    cbMarker = new QCheckBox("Add cable loss marker on S11");
    cbMarker->setChecked(initial.addMarker);
    applyLayout->addWidget(cbMarker);
    layout->addWidget(applyBox);

    lSummary = new QLabel;
    lSummary->setWordWrap(true);
    layout->addWidget(lSummary);

    auto lHint = new QLabel("Calibrate at the near end of the cable, then leave the far end open (or short it). "
                            "All the power that comes back has passed the cable twice, so the one way loss is "
                            "half of the measured return loss. The remaining ripple comes from the imperfect "
                            "open/short and is averaged out; the marker reports it separately.");
    lHint->setWordWrap(true);
    layout->addWidget(lHint);

    auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);

    auto revalidate = [=]() {
        auto &pref = Preferences::getInstance();
        if(eLow->value() < pref.UI.minFrequency) {
            eLow->setValueQuiet(pref.UI.minFrequency);
        }
        if(eHigh->value() > pref.UI.maxFrequency) {
            eHigh->setValueQuiet(pref.UI.maxFrequency);
        }
        if(eHigh->value() <= eLow->value()) {
            eHigh->setValueQuiet(eLow->value() * 1.1);
        }
        if(eLength->value() < 0) {
            eLength->setValueQuiet(0.0);
        }
        updateSummary();
    };
    for(auto e : {eLow, eHigh, eLength}) {
        connect(e, &SIUnitEdit::valueChanged, this, revalidate);
    }
    connect(cbCable, &QComboBox::currentTextChanged, this, &CableLossDialog::updateSummary);
    updateSummary();
}

CableLossDialog::Settings CableLossDialog::settings() const
{
    Settings s;
    s.cableType = cbCable->currentText();
    s.length = eLength->value();
    s.fLow = eLow->value();
    s.fHigh = eHigh->value();
    s.setSweep = cbSweep->isChecked();
    s.createTrace = cbTrace->isChecked();
    s.addMarker = cbMarker->isChecked();
    return s;
}

void CableLossDialog::updateSummary()
{
    auto c = CableLibrary::find(cbCable->currentText());
    if(!c || eLength->value() <= 0) {
        lSummary->setText("Expected loss: unknown (select a cable type and enter its length)");
        return;
    }
    double low = c->lossPerMeter(eLow->value()) * eLength->value();
    double high = c->lossPerMeter(eHigh->value()) * eLength->value();
    lSummary->setText("Expected loss from the datasheet: " + QString::number(low, 'f', 2) + " dB at "
                      + Unit::ToString(eLow->value(), "Hz", " kMG", 4) + ", " + QString::number(high, 'f', 2)
                      + " dB at " + Unit::ToString(eHigh->value(), "Hz", " kMG", 4)
                      + " (return loss twice that, so |S11| should stay well above the noise floor)");
}
