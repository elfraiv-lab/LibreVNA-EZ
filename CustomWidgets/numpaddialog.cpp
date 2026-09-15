#include "numpaddialog.h"

#include "unit.h"
#include "theme.h"

#include <QGridLayout>
#include <QPushButton>
#include <QLabel>
#include <QScreen>
#include <QGuiApplication>
#include <cmath>

NumPadDialog::NumPadDialog(QString unit, QString prefixes, double initial, int precision, QWidget *parent, int maxDecimals)
    : QDialog(parent, Qt::Popup | Qt::FramelessWindowHint),
      unit(unit),
      prefixes(prefixes),
      _value(initial),
      precision(precision),
      maxDecimals(maxDecimals),
      replaceOnInput(true)
{
    setAttribute(Qt::WA_DeleteOnClose, false);

    const int btn = Theme::isTablet() ? 56 : 44;
    QFont big = font();
    big.setPointSize(big.pointSize() + 3);

    auto layout = new QGridLayout(this);
    layout->setSpacing(6);
    layout->setContentsMargins(10, 10, 10, 10);

    display = new QLineEdit;
    display->setReadOnly(true);
    display->setAlignment(Qt::AlignRight);
    display->setFont(big);
    display->setMinimumHeight(btn);
    display->setText(Unit::ToString(initial, unit, prefixes, precision, maxDecimals));
    layout->addWidget(display, 0, 0, 1, 4);

    auto makeButton = [&](const QString &text, int row, int col, int rowSpan = 1, int colSpan = 1) {
        auto b = new QPushButton(text);
        b->setMinimumSize(btn, btn);
        b->setFont(big);
        b->setFocusPolicy(Qt::NoFocus);
        layout->addWidget(b, row, col, rowSpan, colSpan);
        return b;
    };

    // digits
    const char *keys[3][3] = {{"7","8","9"},{"4","5","6"},{"1","2","3"}};
    for(int r=0;r<3;r++) {
        for(int c=0;c<3;c++) {
            auto k = QString(keys[r][c]);
            connect(makeButton(k, r+1, c), &QPushButton::clicked, this, [=](){ append(k); });
        }
    }
    connect(makeButton("0", 4, 0), &QPushButton::clicked, this, [=](){ append("0"); });
    connect(makeButton(".", 4, 1), &QPushButton::clicked, this, [=](){ append("."); });
    connect(makeButton("±", 4, 2), &QPushButton::clicked, this, [=](){
        if(replaceOnInput) { display->clear(); replaceOnInput = false; }
        auto t = display->text();
        if(t.startsWith('-')) t.remove(0, 1); else t.prepend('-');
        display->setText(t);
    });

    // edit column
    auto bBack = makeButton(QString::fromUtf8("\u232B"), 1, 3);   // ⌫
    connect(bBack, &QPushButton::clicked, this, &NumPadDialog::backspace);
    auto bClear = makeButton("C", 2, 3);
    connect(bClear, &QPushButton::clicked, this, [=](){ display->clear(); replaceOnInput = false; });
    auto bCancel = makeButton(QString::fromUtf8("\u2715"), 3, 3);  // ✕
    connect(bCancel, &QPushButton::clicked, this, &QDialog::reject);
    auto bOk = makeButton("OK", 4, 3);
    bOk->setDefault(true);
    connect(bOk, &QPushButton::clicked, this, [=](){ finish(1.0); });

    // SI prefix row: one button per allowed prefix, plus plain unit
    int col = 0;
    auto prefixRow = new QGridLayout;
    prefixRow->setSpacing(6);
    for(auto p : prefixes) {
        QString label = (p == ' ') ? unit : QString(p) + unit;
        if(label.isEmpty()) continue;
        auto b = new QPushButton(label);
        b->setMinimumSize(btn, btn);
        b->setFont(big);
        b->setFocusPolicy(Qt::NoFocus);
        char pc = p.toLatin1();
        connect(b, &QPushButton::clicked, this, [=](){ finish(Unit::SIPrefixToFactor(pc)); });
        prefixRow->addWidget(b, 0, col++);
    }
    if(col > 0) {
        layout->addLayout(prefixRow, 5, 0, 1, 4);
    }
}

void NumPadDialog::append(const QString &s)
{
    if(replaceOnInput) {
        display->clear();
        replaceOnInput = false;
    }
    if(s == "." && display->text().contains('.')) {
        return;
    }
    display->setText(display->text() + s);
}

void NumPadDialog::backspace()
{
    if(replaceOnInput) {
        display->clear();
        replaceOnInput = false;
        return;
    }
    auto t = display->text();
    t.chop(1);
    display->setText(t);
}

void NumPadDialog::finish(double factor)
{
    if(replaceOnInput) {
        // nothing typed, keep old value
        reject();
        return;
    }
    bool ok;
    auto v = display->text().toDouble(&ok);
    if(!ok) {
        reject();
        return;
    }
    _value = v * factor;
    accept();
}

void NumPadDialog::positionNear(QWidget *anchor)
{
    if(!anchor) {
        return;
    }
    adjustSize();
    auto global = anchor->mapToGlobal(QPoint(0, anchor->height()));
    auto screen = QGuiApplication::screenAt(global);
    if(!screen) {
        screen = QGuiApplication::primaryScreen();
    }
    auto avail = screen->availableGeometry();
    QPoint pos = global;
    if(pos.x() + width() > avail.right()) {
        pos.setX(avail.right() - width());
    }
    if(pos.y() + height() > avail.bottom()) {
        // open above the anchor instead
        pos.setY(anchor->mapToGlobal(QPoint(0, 0)).y() - height());
    }
    if(pos.y() < avail.top()) {
        pos.setY(avail.top());
    }
    move(pos);
}

bool NumPadDialog::getValue(QWidget *anchor, QString unit, QString prefixes, double initial, int precision, double &result, int maxDecimals)
{
    NumPadDialog d(unit, prefixes, initial, precision, anchor, maxDecimals);
    d.positionNear(anchor);
    if(d.exec() == QDialog::Accepted) {
        result = d.value();
        return true;
    }
    return false;
}
