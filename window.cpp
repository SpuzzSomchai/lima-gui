/****************************************************************************
**
** Copyright (C) 2021 Anders F Björklund
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the examples of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** BSD License Usage
** Alternatively, you may use this file under the terms of the BSD license
** as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "window.h"
#include "lima.h"

#ifndef QT_NO_SYSTEMTRAYICON

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QCloseEvent>
#include <QDebug>
#include <QFont>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QTableView>
#include <QHeaderView>
#include <QLineEdit>
#include <QMainWindow>
#include <QMenu>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QMessageBox>
#include <QPushButton>
#include <QProcess>
#include <QString>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTextEdit>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFileDialog>
#include <QFileInfo>

#ifndef QT_NO_TERMWIDGET
#include <QApplication>
#include <QMainWindow>
#include "qtermwidget.h"
#endif

#ifndef QT_NO_SOURCEHIGHLITER
#include "qsourcehighliter.h"
using namespace QSourceHighlite;
#endif

//! [0]
Window::Window()
{
    iconComboBox = new QComboBox;
    iconComboBox->addItem(QIcon(":/images/tux.png"), tr("Tux"));
    createInstanceGroupBox();

    createActions();
    createTrayIcon();

    editDir = nullptr;
    editFile = nullptr;

    connect(createButton, &QAbstractButton::clicked, this, &Window::createEditor);
    connect(quickButton, &QAbstractButton::clicked, this, &Window::quickInstance);
    connect(aboutButton, &QAbstractButton::clicked, this, &Window::aboutProgram);
    connect(refreshButton, &QAbstractButton::clicked, this, &Window::updateInstances);

    connect(iconComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &Window::setIcon);

    connect(shellButton, &QAbstractButton::clicked, this, &Window::shellConsole);
    connect(startButton, &QAbstractButton::clicked, this, &Window::startInstance);
    connect(stopButton, &QAbstractButton::clicked, this, &Window::stopInstance);
    connect(editButton, &QAbstractButton::clicked, this, &Window::editInstance);
    connect(removeButton, &QAbstractButton::clicked, this, &Window::removeInstance);

    connect(trayIcon, &QSystemTrayIcon::activated, this, &Window::iconActivated);

    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addWidget(instanceGroupBox);
    setLayout(mainLayout);

    iconComboBox->setCurrentIndex(0);
    trayIcon->show();

    setWindowTitle(tr("Lima"));
    setWindowIcon(*trayIconIcon);
    resize(600, 400);
}
//! [0]

//! [1]
void Window::setVisible(bool visible)
{
    minimizeAction->setEnabled(visible);
    maximizeAction->setEnabled(!isMaximized());
    restoreAction->setEnabled(isMaximized() || !visible);
    QDialog::setVisible(visible);
}
//! [1]

//! [2]
void Window::closeEvent(QCloseEvent *event)
{
#ifdef Q_OS_OSX
    if (!event->spontaneous() || !isVisible()) {
        return;
    }
#endif
    if (trayIcon->isVisible()) {
        QMessageBox::information(this, tr("Systray"),
                                 tr("The program will keep running in the "
                                    "system tray. To terminate the program, "
                                    "choose <b>Quit</b> in the context menu "
                                    "of the system tray entry."));
        hide();
        event->ignore();
    }
}
//! [2]

//! [3]
void Window::setIcon(int index)
{
    QIcon icon = iconComboBox->itemIcon(index);
    trayIcon->setIcon(icon);

    trayIcon->setToolTip(iconComboBox->itemText(index));
}
//! [3]

//! [4]
void Window::iconActivated(QSystemTrayIcon::ActivationReason reason)
{
    switch (reason) {
    case QSystemTrayIcon::Trigger:
    case QSystemTrayIcon::DoubleClick:
        iconComboBox->setCurrentIndex((iconComboBox->currentIndex() + 1) % iconComboBox->count());
        break;
    default:;
    }
}
//! [4]

QString Window::selectedInstance()
{
    QModelIndex index = instanceListView->currentIndex();
    QVariant variant = index.data(Qt::DisplayRole);
    if (variant.isNull()) {
        return QString();
    }
    return variant.toString();
}

void Window::setSelectedInstance(QString instance)
{
    QAbstractItemModel *model = instanceListView->model();
    QModelIndex start = model->index(0, 0);
    QModelIndexList index = model->match(start, Qt::DisplayRole, instance);
    if (index.size() == 0) {
        return;
    }
    instanceListView->setCurrentIndex(index[0]);
}

void Window::shellConsole()
{
    QString instance = selectedInstance();
    QString program = limactlPath();
#ifndef QT_NO_TERMWIDGET
    QMainWindow *mainWindow = new QMainWindow();

    int startnow = 0; // set shell program first
    QTermWidget *console = new QTermWidget(startnow);
    console->setShellProgram(program);
    QStringList args = { "shell", instance };
    console->setArgs(args);
    console->startShellProgram();

    QObject::connect(console, SIGNAL(finished()), mainWindow, SLOT(close()));

    mainWindow->setWindowTitle(QString(tr("lima [%1]")).arg(instance));
    mainWindow->resize(640, 480);
    mainWindow->setCentralWidget(console);
    mainWindow->show();
#else
    QString terminal = qEnvironmentVariable("TERMINAL");
    if (terminal.isEmpty()) {
        terminal = "x-terminal-emulator";
        if (QStandardPaths::findExecutable(terminal).isEmpty()) {
            terminal = "xterm";
        }
    }

    QStringList arguments;
    arguments << "-e" << QString("%1 shell %2").arg(program).arg(instance);
    QProcess *process = new QProcess(this);
    process->start(QStandardPaths::findExecutable(terminal), arguments);
#endif
}

static QString getPrefix()
{
    QString prefix = "/usr/local";
    QString program = limactlPath();
    if (!program.isEmpty()) {
        QString bin = QFileInfo(program).dir().absolutePath();
        prefix = QFileInfo(bin).dir().absolutePath();
    }
    return prefix;
}

void Window::yamlEditor(QString instanceName, QString yamlFile, bool create)
{
    editWindow = new QMainWindow();

    QLabel *label = new QLabel(tr("Name"));
    createName = new QLineEdit(instanceName);
    if (!create) {
        createName->setReadOnly(true);
    }

    QHBoxLayout *topLayout = new QHBoxLayout;
    topLayout->addWidget(label);
    topLayout->addWidget(createName);

    createYAML = new QTextEdit("lima");
    QFont font("monospace");
    font.setStyleHint(QFont::Monospace);
    font.setPointSize(10);
    createYAML->setFont(font);
#ifndef QT_NO_SOURCEHIGHLITER
    QSourceHighliter *highlighter = new QSourceHighliter(createYAML->document());
    highlighter->setCurrentLanguage(QSourceHighliter::CodeYAML);
#endif

    QFile file(yamlFile);
    if (file.open(QFile::ReadOnly | QIODevice::Text)) {
        QString content = QString::fromUtf8(file.readAll());
        createYAML->setPlainText(content);
        file.close();
    }

    QPushButton *cancelButton = new QPushButton(tr("Cancel"));
    QPushButton *loadButton = new QPushButton(tr("Load"));
    QPushButton *saveButton = new QPushButton(tr("Save"));
    QPushButton *okButton = create ? new QPushButton(tr("Create")) : new QPushButton(tr("Ok"));
    okButton->setDefault(true);

    connect(cancelButton, SIGNAL(clicked()), editWindow, SLOT(close()));
    connect(loadButton, &QAbstractButton::clicked, this, &Window::loadYAML);
    connect(saveButton, &QAbstractButton::clicked, this, &Window::saveYAML);
    connect(okButton, &QAbstractButton::clicked, this,
            create ? &Window::createInstance : &Window::updateInstance);

    QHBoxLayout *bottomLayout = new QHBoxLayout;
    bottomLayout->addWidget(cancelButton);
    bottomLayout->addWidget(loadButton);
    bottomLayout->addWidget(saveButton);
    bottomLayout->addWidget(okButton);

    QVBoxLayout *layout = new QVBoxLayout;
    layout->addLayout(topLayout);
    layout->addWidget(createYAML);
    layout->addLayout(bottomLayout);

    QWidget *widget = new QWidget();
    widget->setLayout(layout);

    editWindow->setWindowTitle(tr("lima"));
    editWindow->resize(640, 480);
    editWindow->setCentralWidget(widget);
    editWindow->show();
}

void Window::createEditor()
{
    QString directory = getPrefix() + "/share/doc/lima";
    yamlEditor("default", directory + "/" + defaultYAML(), true);
}

QWidget *Window::newExampleButton(QString name)
{
    Example example = getExamples()[name];
    QIcon icon(QString(":/logos/" + example.logo()));
    QPushButton *button = new QPushButton;
    button->setIconSize(QSize(48, 48));
    button->setIcon(icon);
    QString text = example.text();
    QLabel *textLabel = new QLabel(text);
    textLabel->setAlignment(Qt::AlignCenter);
    QString url = example.url();
    QLabel *urlLabel = new QLabel("<small><a href=\"" + url + "\">" + url + "</a></small>");
    urlLabel->setAlignment(Qt::AlignCenter);
    urlLabel->setOpenExternalLinks(true);
    QVBoxLayout *layout = new QVBoxLayout;
    layout->addWidget(button);
    layout->addWidget(textLabel);
    layout->addWidget(urlLabel);
    connect(button, &QAbstractButton::clicked, this, &Window::quickCreate);
    button->setProperty("name", name);
    QWidget *widget = new QWidget();
    widget->setLayout(layout);
    return widget;
}

void Window::quickCreate()
{
    quickDialog->close();
    QString name = QObject::sender()->property("name").value<QString>();
    Example example = getExamples()[name];
    QString examples = getPrefix() + "/share/doc/lima/examples";
    QString exampleYAML = examples + "/" + example.yaml();
    if (quickPreview->isChecked()) {
        yamlEditor(example.name(), exampleYAML, true);
    } else {
        editWindow = new QMainWindow;
        createName = new QLineEdit;
        createYAML = new QTextEdit;
        readYAML(exampleYAML);
        // TODO: arch, cpus, memory, disk
        createInstance();
    }
}

void Window::urlCreate()
{
    quickDialog->close();
    createInstanceURL();
}

void Window::advancedCreate()
{
    quickDialog->close();
    createEditor();
}

void Window::quickInstance()
{
    quickDialog = new QDialog(this);
    quickDialog->setWindowTitle(tr("Quick Start"));
    quickDialog->setModal(true);

    QPushButton *cancelButton = new QPushButton(tr("Cancel"));
    connect(cancelButton, SIGNAL(clicked()), quickDialog, SLOT(close()));
    QPushButton *urlButton = new QPushButton(tr("URL..."));
    connect(urlButton, &QAbstractButton::clicked, this, &Window::urlCreate);
    QPushButton *advancedButton = new QPushButton(tr("Advanced..."));
    connect(advancedButton, &QAbstractButton::clicked, this, &Window::advancedCreate);

    createURL = new QLineEdit(defaultURL());
    createURL->setFixedWidth(400);
    QFont font;
    font.setPointSize(9);
    createURL->setFont(font);

    QWidget *machineGroupBox = new QWidget();
    QHBoxLayout *machineLayout = new QHBoxLayout;
    machineLayout->addWidget(new QLabel(tr("Arch:")));
    QComboBox *archComboBox = new QComboBox;
    archComboBox->addItem(tr("default"));
    archComboBox->addItem("x86_64");
    archComboBox->addItem("aarch64");
    archComboBox->setEnabled(false); // TODO
    machineLayout->addWidget(archComboBox);
    machineLayout->addWidget(new QLabel(tr("CPUs:")));
    QLineEdit *cpus = new QLineEdit("4");
    cpus->setEnabled(false); // TODO
    machineLayout->addWidget(cpus);
    machineLayout->addWidget(new QLabel(tr("Memory:")));
    QLineEdit *memory = new QLineEdit("4GiB");
    memory->setEnabled(false); // TODO
    machineLayout->addWidget(memory);
    machineLayout->addWidget(new QLabel(tr("Disk:")));
    QLineEdit *disk = new QLineEdit("100GiB");
    disk->setEnabled(false); // TODO
    machineLayout->addWidget(disk);
    quickPreview = new QCheckBox("Edit YAML", this);
    machineLayout->addWidget(quickPreview);
    machineGroupBox->setLayout(machineLayout);

    QGroupBox *distroGroupBox = new QGroupBox(tr("Linux Distributions"));
    QHBoxLayout *distroLayout = new QHBoxLayout;
    distroLayout->addWidget(newExampleButton("alpine"));
    distroLayout->addWidget(newExampleButton("ubuntu"));
    distroLayout->addWidget(newExampleButton("fedora"));
    distroLayout->addWidget(newExampleButton("archlinux"));
    distroGroupBox->setLayout(distroLayout);

    QGroupBox *engineGroupBox = new QGroupBox(tr("Container Engines"));
    QHBoxLayout *engineLayout = new QHBoxLayout;
    engineLayout->addWidget(newExampleButton("nerdctl"));
    engineLayout->addWidget(newExampleButton("docker"));
    engineLayout->addWidget(newExampleButton("podman"));
    engineLayout->addWidget(newExampleButton("singularity"));
    engineGroupBox->setLayout(engineLayout);

    QGroupBox *orchestratorGroupBox = new QGroupBox(tr("Container Orchestration"));
    QHBoxLayout *orchestratorLayout = new QHBoxLayout;
    orchestratorLayout->addWidget(newExampleButton("k3s"));
    orchestratorLayout->addWidget(newExampleButton("k8s"));
    orchestratorLayout->addWidget(newExampleButton("nomad"));
    orchestratorLayout->addWidget(newExampleButton("faasd"));
    orchestratorGroupBox->setLayout(orchestratorLayout);

    QLabel *moreExamplesAvailable =
            new QLabel(tr("There are more examples available. "
                          "Use the \"Load\" button (under Advanced), to see them.<br>"));

    QVBoxLayout *topLayout = new QVBoxLayout;
    topLayout->addWidget(machineGroupBox);
    topLayout->addWidget(distroGroupBox);
    topLayout->addWidget(engineGroupBox);
    topLayout->addWidget(orchestratorGroupBox);
    topLayout->addWidget(moreExamplesAvailable);

    QHBoxLayout *bottomLayout = new QHBoxLayout;
    bottomLayout->addWidget(cancelButton);
    bottomLayout->addStretch();
    bottomLayout->addWidget(urlButton);
    bottomLayout->addWidget(createURL);
    bottomLayout->addWidget(advancedButton);

    QVBoxLayout *layout = new QVBoxLayout;
    layout->addLayout(topLayout);
    layout->addLayout(bottomLayout);

    quickDialog->resize(640, 480);
    quickDialog->setLayout(layout);
    quickDialog->exec();
}

bool Window::getProcessOutput(QStringList arguments, QString &text)
{
    bool success;

    QString program = limactlPath();

    QProcess *process = new QProcess(this);
    process->start(program, arguments);
    success = process->waitForFinished();
    if (success) {
        text = process->readAllStandardOutput();
    } else {
        qDebug() << process->readAllStandardError();
    }
    delete process;
    return success;
}

QString Window::getVersion()
{
    QString program = limactlPath();
    if (!program.isEmpty()) {
        QStringList arguments;
        arguments << "--version";
        QString text;
        bool success = getProcessOutput(arguments, text);
        if (success) {
            text.replace("limactl version", "");
            return text.trimmed();
        }
    }
    return "N/A";
}

InstanceList Window::getInstances()
{
    InstanceList instances;
    QStringList arguments;
    arguments << "list"
              << "--json";
    QString text;
    bool success = getProcessOutput(arguments, text);
    QStringList lines;
    if (success) {
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
        lines = text.split("\n", Qt::SkipEmptyParts);
#else
        lines = text.split("\n", QString::SkipEmptyParts);
#endif
    }
    for (int i = 0; i < lines.size(); i++) {
        QString line = lines.at(i);
        QJsonParseError error;
        QJsonDocument json = QJsonDocument::fromJson(line.toUtf8(), &error);
        if (json.isNull()) {
            qDebug() << error.errorString();
            continue;
        }
        if (json.isObject()) {
            QJsonObject obj = json.object();
            QString name;
            if (obj.contains("name")) {
                name = obj["name"].toString();
            }
            if (name.isEmpty()) {
                continue;
            }
            Instance instance(name);
            if (obj.contains("status")) {
                QString status = obj["status"].toString();
                instance.setStatus(status);
            }
            if (obj.contains("dir")) {
                QString dir = obj["dir"].toString();
                instance.setDir(dir);
            }
            if (obj.contains("arch")) {
                QString arch = obj["arch"].toString();
                instance.setArch(arch);
            }
            if (obj.contains("cpus")) {
                int cpus = obj["cpus"].toInt();
                instance.setCpus(cpus);
            }
            if (obj.contains("memory")) {
                double memory = obj["memory"].toDouble();
                instance.setMemory((qint64)memory);
            }
            if (obj.contains("disk")) {
                double disk = obj["disk"].toDouble();
                instance.setDisk((qint64)disk);
            }
            instances << instance;
        }
    }
    return instances;
}

InstanceHash Window::getInstanceHash()
{
    InstanceList instances = getInstances();
    InstanceHash instanceHash;
    for (int i = 0; i < instances.size(); i++) {
        Instance instance = instances.at(i);
        instanceHash[instance.name()] = instance;
    }
    return instanceHash;
}

void Window::createInstanceGroupBox()
{
    instanceGroupBox = new QGroupBox(tr("Instances"));

    InstanceList instances = getInstances();
    instanceModel = new InstanceModel(instances);

    instanceListView = new QTableView();
    instanceListView->setModel(instanceModel);
    instanceListView->setSelectionMode(QAbstractItemView::SingleSelection);
    instanceListView->setSelectionBehavior(QAbstractItemView::SelectRows);
    instanceListView->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    instanceListView->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    instanceListView->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    instanceListView->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    instanceListView->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    instanceListView->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    setSelectedInstance("default");

    connect(instanceListView, SIGNAL(clicked(const QModelIndex &)), this, SLOT(updateButtons()));

    createButton = new QPushButton(tr("Create"));
    createButton->setIcon(QIcon(":/images/add.png"));
    quickButton = new QPushButton(tr("Quick"));
    quickButton->setIcon(QIcon(":/images/quick.png"));
    aboutButton = new QPushButton(tr("About"));
    aboutButton->setIcon(QIcon(":/images/info.png"));
    refreshButton = new QPushButton(tr("Refresh"));
    refreshButton->setIcon(QIcon(":/images/reload.png"));

    shellButton = new QPushButton(tr("Shell"));
    shellButton->setIcon(QIcon(":/images/terminal.png"));
    startButton = new QPushButton(tr("Start"));
    stopButton = new QPushButton(tr("Stop"));
    editButton = new QPushButton(tr("Edit"));
    removeButton = new QPushButton(tr("Remove"));
    removeButton->setIcon(QIcon(":/images/remove.png"));

    updateButtons();

    QHBoxLayout *refreshButtonLayout = new QHBoxLayout;
    refreshButtonLayout->addWidget(createButton);
    refreshButtonLayout->addWidget(quickButton);
    refreshButtonLayout->addStretch();
    refreshButtonLayout->addWidget(aboutButton);
    refreshButtonLayout->addWidget(refreshButton);

    QHBoxLayout *instanceButtonLayout = new QHBoxLayout;
    instanceButtonLayout->addWidget(shellButton);
    instanceButtonLayout->addWidget(startButton);
    instanceButtonLayout->addWidget(stopButton);
    instanceButtonLayout->addWidget(editButton);
    instanceButtonLayout->addWidget(removeButton);

    QVBoxLayout *instanceLayout = new QVBoxLayout;
    instanceLayout->addLayout(refreshButtonLayout);
    instanceLayout->addWidget(instanceListView);
    instanceLayout->addLayout(instanceButtonLayout);
    instanceGroupBox->setLayout(instanceLayout);
}

void Window::updateButtons()
{
    QString inst = selectedInstance();
    if (inst.isEmpty()) {
        shellButton->setEnabled(false);
        startButton->setEnabled(false);
        stopButton->setEnabled(false);
        editButton->setEnabled(false);
        removeButton->setEnabled(false);
        return;
    }
    Instance instance = getInstanceHash()[inst];
    if (instance.status() == "Running") {
        shellButton->setEnabled(true);
        startButton->setEnabled(false);
        stopButton->setEnabled(true);
        editButton->setEnabled(false);
        removeButton->setEnabled(false);
    } else if (instance.status() == "Stopped") {
        shellButton->setEnabled(false);
        startButton->setEnabled(true);
        stopButton->setEnabled(false);
        editButton->setEnabled(true);
        removeButton->setEnabled(true);
    }
}

void Window::aboutProgram()
{
    QString url = "https://github.com/lima-vm/lima";
    QMessageBox::about(this, tr("Lima") + " version " + getVersion(),
                       tr("Lima runs Linux Virtual Machines") + "<br>" + "<br>" + "<a href=\"" + url
                               + "\">" + url + "</a>");
}

void Window::updateInstances()
{
    QString instance = selectedInstance();
    instanceModel->setInstances(getInstances());
    setSelectedInstance(instance);
}

void Window::sendCommand(QString cmd)
{
    return sendCommand(QStringList(cmd));
}

void Window::sendCommand(QStringList arguments)
{
    QString program = limactlPath();
    process = new QProcess(this);
    connect(process, SIGNAL(started()), this, SLOT(startedCommand()));
    connect(process, SIGNAL(finished(int, QProcess::ExitStatus)), this,
            SLOT(finishedCommand(int, QProcess::ExitStatus)));
    process->start(program, arguments);
}

void Window::startedCommand()
{
    this->setCursor(Qt::WaitCursor);
}

void Window::finishedCommand(int code, QProcess::ExitStatus status)
{
    this->unsetCursor();
    QString command = process->program() + " " + process->arguments().join(" ");
    if (status == QProcess::NormalExit && code != 0) {
        QMessageBox msgBox;
        msgBox.setIcon(QMessageBox::Warning);
        msgBox.setText("<b>" + command + "</b>");
        msgBox.setInformativeText(process->readAllStandardError());
        msgBox.exec();
    } else {
        QByteArray output = process->readAllStandardOutput();
        if (output.length() > 0) {
            QMessageBox msgBox;
            msgBox.setIcon(QMessageBox::Information);
            msgBox.setText("<b>" + command + "</b>");
            msgBox.setInformativeText(output);
            msgBox.exec();
        }
    }

    if (editFile) {
        editFile->remove();
        delete editFile;
        editFile = nullptr;
        delete editDir;
        editDir = nullptr;
    }
}

void Window::loadYAML()
{
    QString examples = getPrefix() + "/share/doc/lima/examples";

    QString fileName = QFileDialog::getOpenFileName(this, tr("Open YAML"), examples,
                                                    tr("YAML Files (*.yaml)"));
    if (fileName.isEmpty()) {
        return;
    }
    readYAML(fileName);
}

void Window::readYAML(QString fileName)
{
    QString baseName = QFileInfo(fileName).baseName();
    createName->setText(baseName.replace(".yaml", ""));
    QFile file(fileName);
    if (file.open(QFile::ReadOnly | QIODevice::Text)) {
        QString content = QString::fromUtf8(file.readAll());
        createYAML->setPlainText(content);
        file.close();
    }
}

void Window::saveYAML()
{
    QString baseName = createName->text() + ".yaml";
    QString fileName = QFileDialog::getSaveFileName(this, tr("Save YAML"), baseName,
                                                    tr("YAML Files (*.yaml)"));
    if (fileName.isEmpty()) {
        return;
    }
    writeYAML(fileName);
}

void Window::writeYAML(QString fileName)
{
    QFile file(fileName);
    if (file.open(QFile::WriteOnly | QIODevice::Text)) {
        QString yaml = createYAML->toPlainText();
        file.write(yaml.toUtf8());
        file.close();
    }
}

QFile *Window::validateYAML(QString name)
{
    editDir = new QTemporaryDir;
    if (!editDir->isValid()) {
        QMessageBox::warning(this, tr("lima"), tr("Could not create temporary dir"));
        return 0;
    }
    QFile *temp = new QFile(editDir->path() + "/" + QString("%1.yaml").arg(name));
    if (temp->open(QFile::WriteOnly | QIODevice::Text)) {
        QString yaml = createYAML->toPlainText();
        temp->write(yaml.toUtf8());
        temp->close();
    }
    QProcess process(this);
    QString program = limactlPath();
    process.start(program, { "validate", temp->fileName() });
    bool success = process.waitForFinished();
    if (success) {
        if (process.exitStatus() == QProcess::NormalExit && process.exitCode() != 0) {
            QMessageBox msgBox;
            msgBox.setIcon(QMessageBox::Critical);
            msgBox.setText(tr("Validation failed!"));
            msgBox.setInformativeText(process.readAllStandardError());
            msgBox.exec();
            delete temp;
            return 0;
        }
    }
    return temp;
}

void Window::createInstance()
{
    QString name = createName->text();
    editFile = validateYAML(name);
    if (!editFile)
        return;
    editWindow->close();
    QStringList args = { "start", "--tty=false", editFile->fileName() };
    sendCommand(args);

    updateInstances();
}

void Window::createInstanceURL()
{
    QString url = createURL->text();
    if (url.startsWith("https://github.com")) {
        // Get the "raw" YAML, and not the prettified HTTP
        url = url.replace("github.com", "raw.githubusercontent.com");
        url = url.replace("blob/", "");
    }
    QStringList args = { "start", "--tty=false", url };
    sendCommand(args);

    updateInstances();
}

void Window::updateInstance()
{
    QString name = createName->text();
    editFile = validateYAML(name);
    if (!editFile)
        return;
    editWindow->close();
    Instance instance = getInstanceHash()[name];
    QString yamlFile = instance.dir() + "/" + "lima.yaml";
    QFile(yamlFile).remove();
    editFile->rename(yamlFile);
}

void Window::startInstance()
{
    QString instance = selectedInstance();
    QStringList args = { "start", instance };
    sendCommand(args);
    // updateInstances();
}

void Window::stopInstance()
{
    QString instance = selectedInstance();
    QStringList args = { "stop", instance };
    sendCommand(args);
    // updateInstances();
}

void Window::editInstance()
{
    QString name = selectedInstance();
    Instance instance = getInstanceHash()[name];
    QString yamlFile = instance.dir() + "/" + "lima.yaml";
    yamlEditor(instance.name(), yamlFile, false);
}

bool Window::askConfirm(QString instance)
{
    QMessageBox msgBox;
    msgBox.setWindowTitle(tr("lima"));
    msgBox.setIcon(QMessageBox::Critical);
    msgBox.setText(tr("Remove an existing instance"));
    msgBox.setInformativeText(tr("Are you sure you want to remove '%1' ? "
                                 "This will delete all files for the instance.")
                                      .arg(instance));
    msgBox.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
    msgBox.setDefaultButton(QMessageBox::Cancel);
    return msgBox.exec() == QMessageBox::Ok;
}

void Window::removeInstance()
{
    QString instance = selectedInstance();
    if (askConfirm(instance)) {
        QStringList args = { "rm", "--force", instance };
        sendCommand(args);
        updateInstances();
    }
}

void Window::createActions()
{
    minimizeAction = new QAction(tr("Mi&nimize"), this);
    connect(minimizeAction, &QAction::triggered, this, &QWidget::hide);

    maximizeAction = new QAction(tr("Ma&ximize"), this);
    connect(maximizeAction, &QAction::triggered, this, &QWidget::showMaximized);

    restoreAction = new QAction(tr("&Restore"), this);
    connect(restoreAction, &QAction::triggered, this, &QWidget::showNormal);

    aboutAction = new QAction(tr("About"), this);
    connect(aboutAction, &QAction::triggered, this, &Window::aboutProgram);

    aboutQtAction = new QAction(tr("About &Qt"), this);
    connect(aboutQtAction, &QAction::triggered, this, &QApplication::aboutQt);

    quitAction = new QAction(tr("&Quit"), this);
    connect(quitAction, &QAction::triggered, qApp, &QCoreApplication::quit);
}

void Window::createTrayIcon()
{
    trayIconMenu = new QMenu(this);
    trayIconMenu->addAction(minimizeAction);
    trayIconMenu->addAction(maximizeAction);
    trayIconMenu->addAction(restoreAction);
    trayIconMenu->addSeparator();
    trayIconMenu->addAction(aboutAction);
    trayIconMenu->addAction(aboutQtAction);
    trayIconMenu->addAction(quitAction);

    trayIconIcon = new QIcon(":/images/tux.png");

    trayIcon = new QSystemTrayIcon(this);
    trayIcon->setContextMenu(trayIconMenu);
    trayIcon->setIcon(*trayIconIcon);
}

#endif
