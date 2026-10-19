#include <QApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QElapsedTimer>
#include <QKeyEvent>
#include <QLineEdit>
#include <QPushButton>
#include <QStandardPaths>
#include <QThread>

#include "application/connectionsession.h"
#include "infrastructure/coreprocess/zjuconnectprocess.h"
#include "presentation/coordinators/authdialogcoordinator.h"

namespace
{
template <typename Predicate> bool waitFor(Predicate predicate)
{
    QElapsedTimer timer;
    timer.start();
    do
    {
        QCoreApplication::processEvents();
        if (predicate()) return true;
        QThread::msleep(5);
    } while (timer.elapsed() < 5000);
    qCritical() << "Timed out waiting for auth flow";
    return false;
}

struct Fixture
{
    QWidget parent;
    AuthDialogCoordinator dialogs{&parent, nullptr};
    ConnectionSession session{new ZjuConnectProcess};
    int prompts = 0;
    int submissions = 0;
    int cancellations = 0;
    QString output;

    Fixture()
    {
        parent.show();
        QObject::connect(&session, &ConnectionSession::totpCode, &dialogs, [this]()
        {
            ++prompts;
            dialogs.requestTotpCode();
        });
        QObject::connect(&dialogs, &AuthDialogCoordinator::interactiveInputSubmitted,
                         &session, [this](const QByteArray &input)
        {
            ++submissions;
            session.submitInput(input);
        });
        QObject::connect(&dialogs, &AuthDialogCoordinator::interactiveInputCancelled,
                         &session, [this]()
        {
            ++cancellations;
            session.cancelInteractiveInput();
        });
        QObject::connect(&session, &ConnectionSession::stateChanged, &dialogs,
                         [this](ConnectionState state)
        {
            if (state != ConnectionState::Starting) dialogs.dismissTotpCode();
        });
        QObject::connect(&session, &ConnectionSession::outputRead, &dialogs,
                         [this](const QString &text) { output += text; });
    }

    QDialog *dialog()
    {
        for (auto *dialog : parent.findChildren<QDialog *>("totpDialog"))
            if (dialog->isVisible()) return dialog;
        return nullptr;
    }

    bool start(const char *mode)
    {
        qputenv("EZ4CONNECT_TEST_AUTH_MODE", mode);
        ConnectionProfile profile;
        profile.program = QCoreApplication::applicationDirPath() + "/fakeauthcore";
#ifdef Q_OS_WIN
        profile.program += ".exe";
#endif
        profile.endpoint.protocol = mode == QByteArray("easyconnect") ? "easyconnect" : "atrust";
        return session.start(profile, {});
    }

    bool stop()
    {
        session.stop();
        return waitFor([this]() { return session.state() == ConnectionState::Disconnected; });
    }
};

bool roundTrip(const char *mode)
{
    qInfo() << "Testing TOTP round trip:" << mode;
    Fixture f;
    if (!f.start(mode) || !waitFor([&]() { return f.dialog() != nullptr; })) return false;
    // Wait until both copies have actually been parsed, while the input is pending.
    if (!waitFor([&]() { return f.output.count("code:") + f.output.count("token:") >= 3; })) return false;
    auto *dialog = f.dialog();
    auto *edit = dialog->findChild<QLineEdit *>();
    auto *ok = dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok);
    if (f.prompts != 1 || ok->isEnabled() || dialog->isModal()
        || dialog->windowTitle() != QStringLiteral("双因素身份验证")
        || !edit->hasFocus()) return false;
    f.dialogs.requestTotpCode();
    if (f.dialog() != dialog) return false;
    for (const QString text : {"12345", "1234567", "abcdef"})
    {
        edit->setText(text);
        if (ok->isEnabled()) return false;
    }
    edit->setText("12345678");
    if (!ok->isEnabled()) return false;
    edit->setText("123456");
    // Enter must follow the same accepted path as clicking Confirm.
    QKeyEvent enter(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
    QApplication::sendEvent(edit, &enter);
    if (!waitFor([&]() { return f.prompts == 2 && f.dialog() != nullptr; })) return false;
    edit = f.dialog()->findChild<QLineEdit *>();
    if (!edit->text().isEmpty()) return false;
    edit->setText("654321");
    f.dialog()->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->click();
    if (!waitFor([&]() { return f.session.state() == ConnectionState::Running; })) return false;
    if (f.submissions != 2 || f.cancellations != 0 || f.dialog()
        || f.output.contains("123456") || f.output.contains("654321")) return false;
    qInfo() << "Stopping connected fake core";
    return f.stop();
}

bool teardownAndRestart()
{
    qInfo() << "Testing cancellation and session teardown";
    Fixture f;
    // Cancellation, disconnection, process exit, partial output and fresh reconnect.
    if (!f.start("atrust") || !waitFor([&]() { return f.dialog() != nullptr; })) return false;
    f.dialog()->reject();
    if (!waitFor([&]() { return f.session.state() == ConnectionState::Disconnected; })
        || f.cancellations != 1 || f.submissions != 0) return false;
    if (!f.start("atrust") || !waitFor([&]() { return f.dialog() != nullptr; })) return false;
    QPointer<QDialog> oldDialog = f.dialog();
    oldDialog->findChild<QLineEdit *>()->setText("123456");
    if (!f.stop() || f.dialog() || f.submissions != 0 || f.cancellations != 1) return false;
    if (oldDialog) oldDialog->accept(); // Disconnected old signals must be inert.
    if (f.submissions != 0) return false;
    if (!f.start("exit") || !waitFor([&]() { return f.dialog() != nullptr; })) return false;
    if (!waitFor([&]() { return f.session.state() == ConnectionState::Disconnected; })
        || f.dialog() || f.cancellations != 1) return false;
    const int before = f.prompts;
    if (!f.start("partial")
        || !waitFor([&]() { return f.session.state() == ConnectionState::Disconnected; })
        || f.prompts != before) return false;
    if (!f.start("atrust") || !waitFor([&]() { return f.dialog() != nullptr; })) return false;
    if (!f.dialog()->findChild<QLineEdit *>()->text().isEmpty()) return false;
    f.dialogs.setSettings(nullptr);
    if (f.dialog() || f.cancellations != 1) return false;
    return f.stop();
}
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    QStandardPaths::setTestModeEnabled(true);
    const bool ok = roundTrip("atrust") && roundTrip("easyconnect") && teardownAndRestart();
    if (!ok) qCritical() << "TOTP integration test failed";
    return ok ? 0 : 1;
}
