#pragma once

#include <QtWidgets/QMainWindow>
#include <qprocess.h>
#include "ui_RunGUI.h"
#include <yaml-cpp/yaml.h>
#include <filesystem>

class RunGUI : public QMainWindow
{
    Q_OBJECT

public:
    RunGUI(QWidget *parent = nullptr);
    ~RunGUI();
private slots:
    void on_select_directory();
    void on_start_run();
    void on_stop_run();
private:
    bool dump_to_config_file();
    QString parseAnsiToHtml(const QString& ansiText);
    QProcess process;
    Ui::RunGUIClass ui;
    YAML::Node config, settings;
    std::filesystem::path directory_path;
};
