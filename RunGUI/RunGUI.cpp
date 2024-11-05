#include "RunGUI.h"
#include <qfiledialog.h>
#include <qmessagebox.h>
#include <qregularexpression.h>
#include <QtTextToSpeech/qtexttospeech.h>
#include <fstream>
#ifdef Q_OS_WIN
#include<Windows.h>
#endif
#ifdef Q_OS_LINUX
#include<unistd.h>
#endif
RunGUI::RunGUI(QWidget *parent)
    : QMainWindow(parent)
{
    namespace fs = std::filesystem;
    ui.setupUi(this);
    connect(ui.select_directory_button, &QPushButton::clicked, this, &RunGUI::on_select_directory);
    connect(ui.start_run_button, &QPushButton::clicked, this, &RunGUI::on_start_run);
    connect(ui.stop_run, &QPushButton::clicked, this, &RunGUI::on_stop_run);
    connect(&process, &QProcess::finished, [this] {
        if (process.waitForFinished()) {
            ui.ErrorOutput->append(tr("RealRun进程已终止。\n"));
            process.close();
            ui.stop_run->setEnabled(false);
        }
    });
    if (fs::exists("settings.yaml")) {
        try {
            settings = YAML::LoadFile("settings.yaml");
            directory_path = settings["directory_path"].as<std::string>();
            if (fs::exists(directory_path)) {
                if (auto config_path = directory_path / "config.yaml"; fs::exists(config_path)) {
                    ui.DirectoryPath->setText(QString::fromStdString(directory_path.make_preferred().string()));
                    ui.ConfigPath->setText(QString::fromStdString(config_path.make_preferred().string()));
                    config = YAML::LoadFile(config_path.string());
                    auto speed = config["v"].as<float>();
                    auto route_config = config["routeConfig"].as<std::string>();
                    ui.RunSpeed->setText(QString::number(speed));
                    ui.RouteFileName->setText(QString::fromStdString(route_config));
                    if (!fs::exists(directory_path / "main.py")) {
                        throw std::runtime_error(tr("目录中没有找到main.py！").toStdString());
                    }
                }
            }
        }
        catch (const std::exception&) {}
    }
    else {
        std::ofstream fout("settings.yaml");
        settings["directory_path"] = "";
        fout << settings;
        fout.close();
    }
    ui.stop_run->setEnabled(false);
    
}

RunGUI::~RunGUI()
{}

void RunGUI::closeEvent(QCloseEvent * event)
{
    if (ui.stop_run->isEnabled()) {
        on_stop_run();
    }
    event->accept();
}

void RunGUI::on_start_run()
{
    if (!dump_to_config_file())
        return;

    QObject::connect(&process, &QProcess::readyReadStandardOutput, [this]() {
        QByteArray output = process.readAllStandardOutput();
        QString outputStr = QString::fromLocal8Bit(output);
        ui.ErrorOutput->append(parseAnsiToHtml(outputStr));
        });

    QObject::connect(&process, &QProcess::readyReadStandardError, [this]() {
        QByteArray errorOutput = process.readAllStandardError();
        ui.ErrorOutput->append(parseAnsiToHtml(errorOutput));
        });
    process.setWorkingDirectory(QString::fromStdString(directory_path.string()));
    process.start("python", { QString::fromStdString((directory_path / "main.py").string()) });
    if (!process.waitForStarted())
    {
        QMessageBox::critical(
            this, 
            "启动失败", 
            "无法启动RealRun进程", QMessageBox::Ok);
        ui.stop_run->setEnabled(false);
        ui.start_run_button->setEnabled(true);
        return;
    }
    else {
        ui.statusBar->showMessage(tr("正在跑步······"));
        ui.stop_run->setEnabled(true);
        ui.start_run_button->setEnabled(false);
    }
}

void RunGUI::on_stop_run() {
#ifdef Q_OS_WIN
    qint64 pid = process.processId();
    if (AttachConsole(pid)) {
        GenerateConsoleCtrlEvent(CTRL_C_EVENT, pid);
        FreeConsole();
    }
#endif // Q_OS_WIN
#ifdef Q_OS_LINUX
    qint64 pid = process.processId();
    kill(pid, SIGINT);
#endif // Q_OS_LINUX
    if (process.waitForFinished()) {
        ui.ErrorOutput->append(tr("RealRun进程已终止。\n"));
        process.close();
        ui.stop_run->setEnabled(false);
        ui.start_run_button->setEnabled(true);
        ui.statusBar->showMessage(tr("跑步已终止"));
    }
    else {
        ui.ErrorOutput->append(tr("RealRun进程终止失败！\n"));
        ui.stop_run->setEnabled(true);
        ui.start_run_button->setEnabled(false);
    }
}

bool RunGUI::dump_to_config_file()
{
    try {
        auto config_path = directory_path / "config.yaml";
        config = YAML::LoadFile(config_path.string());
        config["v"] = ui.RunSpeed->text().toFloat();
        config["routeConfig"] = ui.RouteFileName->text().toStdString();
        std::ofstream fout(config_path.make_preferred());
        fout << config;
        fout.close();
        return true;
    }
    catch(const std::exception& e){
        ui.ErrorOutput->setText(e.what());
        QMessageBox::critical(this, tr("写入配置文件错误"), tr(e.what()), QMessageBox::Ok);
        return false;
    }
}

void RunGUI::on_select_directory() {
    namespace fs = std::filesystem;
    QString Qfile_path= QFileDialog::getExistingDirectory(this, tr("选择文件夹"), QDir::homePath());
    directory_path = Qfile_path.toStdString();
    try {
        if (fs::exists(directory_path)) {

            settings["directory_path"] = directory_path.string();
            std::ofstream fout("settings.yaml");
            fout << settings;
            fout.close();

            if (auto config_path = directory_path / "config.yaml"; fs::exists(config_path)) {
                ui.DirectoryPath->setText(QString::fromStdString(directory_path.make_preferred().string()));
                ui.ConfigPath->setText(QString::fromStdString(config_path.make_preferred().string()));
                config = YAML::LoadFile(config_path.string());
                auto speed = config["v"].as<float>();
                auto route_config = config["routeConfig"].as<std::string>();
                ui.RunSpeed->setText(QString::number(speed));
                ui.RouteFileName->setText(QString::fromStdString(route_config));
                if (!fs::exists(directory_path / "main.py")) {
                    throw std::runtime_error(tr("目录中没有找到main.py！").toStdString());
                }
            }
        }
    }
    catch (const std::exception& e) {
        using namespace std::literals;
        QMessageBox::critical(
            this,
            tr("无效的目录"),
            e.what(), QMessageBox::Ok);
        directory_path.clear();
        config.reset();
        ui.ErrorOutput->setText(QString::fromStdString("错误："s + e.what()));
        ui.ConfigPath->setText(QString("暂无"));
        ui.RunSpeed->setText(QString("0"));
        ui.RouteFileName->setText(QString("暂无"));
    }
}

QString RunGUI::parseAnsiToHtml(const QString& ansiText)
{
    QString htmlText;
    static const QRegularExpression ansiRegex(R"(\x1B\[([0-9;]*)([A-Za-z]))");
    int lastPos = 0;
    QRegularExpressionMatch match;

    while ((match = ansiRegex.match(ansiText, lastPos)).hasMatch()) {
        htmlText += ansiText.mid(lastPos, match.capturedStart() - lastPos);
        QStringList codes = match.captured(1).split(';');
        QString closingTag;

        for (const QString& code : codes) {
            bool ok;
            int codeInt = code.toInt(&ok);
            if (!ok) continue;

            switch (codeInt) {
            case 0: closingTag = "</span>"; break;
            case 30: htmlText += "<span style=\"color:black;\">"; break;
            case 31: htmlText += "<span style=\"color:red;\">"; break;
            case 32: htmlText += "<span style=\"color:green;\">"; break;
            case 33: htmlText += "<span style=\"color:yellow;\">"; break;
            case 34: htmlText += "<span style=\"color:blue;\">"; break;
            case 35: htmlText += "<span style=\"color:magenta;\">"; break;
            case 36: htmlText += "<span style=\"color:cyan;\">"; break;
            case 37: htmlText += "<span style=\"color:white;\">"; break;
            case 40: htmlText += "<span style=\"background-color:black;\">"; break;
            case 41: htmlText += "<span style=\"background-color:red;\">"; break;
            case 42: htmlText += "<span style=\"background-color:green;\">"; break;
            case 43: htmlText += "<span style=\"background-color:yellow;\">"; break;
            case 44: htmlText += "<span style=\"background-color:blue;\">"; break;
            case 45: htmlText += "<span style=\"background-color:magenta;\">"; break;
            case 46: htmlText += "<span style=\"background-color:cyan;\">"; break;
            case 47: htmlText += "<span style=\"background-color:white;\">"; break;
            default: break;
            }
        }

        if (!closingTag.isEmpty()) {
            htmlText += closingTag;
        }

        lastPos = match.capturedEnd();
    }

    htmlText += ansiText.mid(lastPos);
    return htmlText;
}