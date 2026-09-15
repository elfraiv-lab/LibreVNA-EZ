#include "cableimpedancedialog.h"

#include "CustomWidgets/siunitedit.h"
#include "cablelibrary.h"

#include <QVBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QDialogButtonBox>
#include <QPushButton>

CableImpedanceDialog::CableImpedanceDialog(const Settings &initial, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Cable impedance (TDR)");
    setAttribute(Qt::WA_DeleteOnClose);

    auto layout = new QVBoxLayout(this);

    auto modeBox = new QGroupBox("Known quantity");
    auto modeLayout = new QFormLayout(modeBox);
    cbCable = new QComboBox;
    cbCable->addItems(CableLibrary::names());
    cbCable->addItem(CableLibrary::customName());
    cbCable->setCurrentText(initial.cableType.isEmpty() ? CableLibrary::customName() : initial.cableType);
    cbCable->setToolTip("Cable type from the library: sets the datasheet velocity factor");
    modeLayout->addRow("Cable:", cbCable);
    rbLength = new QRadioButton("Cable length is known, compute velocity factor");
    rbVF = new QRadioButton("Velocity factor is known, compute length");
    rbLength->setChecked(initial.knownLength);
    rbVF->setChecked(!initial.knownLength);
    modeLayout->addRow(rbLength);
    eLength = new SIUnitEdit("m", " ", 4);
    eLength->setValue(initial.length);
    eLength->setToolTip("Physical length of the cable section");
    modeLayout->addRow("Length:", eLength);
    modeLayout->addRow(rbVF);
    eVF = new SIUnitEdit("", " ", 3);
    eVF->setValue(initial.velocityFactor);
    eVF->setToolTip("Velocity factor of the cable (e.g. 0.66 for solid PE, 0.82 for foam PE)");
    modeLayout->addRow("Velocity factor:", eVF);
    layout->addWidget(modeBox);

    auto searchBox = new QGroupBox("Flat section search");
    auto searchLayout = new QFormLayout(searchBox);
    eTolerance = new SIUnitEdit("Ω", " ", 3);
    eTolerance->setValue(initial.zTolerance);
    eTolerance->setToolTip("The cable is the longest stretch where the impedance stays within this tolerance");
    searchLayout->addRow("Z tolerance:", eTolerance);
    layout->addWidget(searchBox);

    auto applyBox = new QGroupBox("Apply");
    auto applyLayout = new QVBoxLayout(applyBox);
    cbSweep = new QCheckBox("Set TDR sweep (evenly spaced from f_step to max, 1000 points)");
    cbSweep->setChecked(initial.setSweep);
    applyLayout->addWidget(cbSweep);
    cbTrace = new QCheckBox("Create S11 trace with TDR step response and impedance vs. distance graph");
    cbTrace->setChecked(initial.createTrace);
    applyLayout->addWidget(cbTrace);
    cbMarker = new QCheckBox("Add cable impedance marker");
    cbMarker->setChecked(initial.addMarker);
    applyLayout->addWidget(cbMarker);
    layout->addWidget(applyBox);

    lHint = new QLabel("Calibrate at the end of the test cable (or directly at the port) and leave the far end of "
                       "the cable under test open. The marker averages the impedance over the flat section, "
                       "its helper markers can be dragged to adjust the section.");
    lHint->setWordWrap(true);
    layout->addWidget(lHint);

    auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);

    auto revalidate = [=]() {
        if(eLength->value() <= 0) {
            eLength->setValueQuiet(1.0);
        }
        if(eVF->value() <= 0.05) {
            eVF->setValueQuiet(0.05);
        } else if(eVF->value() > 1.0) {
            eVF->setValueQuiet(1.0);
        }
        if(eTolerance->value() < 0.1) {
            eTolerance->setValueQuiet(0.1);
        }
    };
    for(auto e : {eLength, eVF, eTolerance}) {
        connect(e, &SIUnitEdit::valueChanged, this, revalidate);
    }
    connect(rbLength, &QRadioButton::toggled, this, &CableImpedanceDialog::updateEnabled);
    connect(cbCable, &QComboBox::currentTextChanged, this, [=](const QString &name) {
        if(auto c = CableLibrary::find(name)) {
            eVF->setValue(c->vf);
        }
    });
    if(auto c = CableLibrary::find(cbCable->currentText())) {
        eVF->setValue(c->vf);
    }
    updateEnabled();
}

CableImpedanceDialog::Settings CableImpedanceDialog::settings() const
{
    Settings s;
    s.cableType = cbCable->currentText();
    auto c = CableLibrary::find(s.cableType);
    s.nominalVF = c ? c->vf : 0.0;
    s.knownLength = rbLength->isChecked();
    s.length = eLength->value();
    s.velocityFactor = eVF->value();
    s.zTolerance = eTolerance->value();
    s.setSweep = cbSweep->isChecked();
    s.createTrace = cbTrace->isChecked();
    s.addMarker = cbMarker->isChecked();
    return s;
}

void CableImpedanceDialog::updateEnabled()
{
    bool lengthKnown = rbLength->isChecked();
    eLength->setEnabled(lengthKnown);
    eVF->setEnabled(!lengthKnown);
}
