#include "RundownPresetWidget.h"

#include "Global.h"

#include "DatabaseManager.h"
#include "TriCasterDeviceManager.h"
#include "GpiManager.h"
#include "Events/ConnectionStateChangedEvent.h"
#include "Events/Inspector/LabelChangedEvent.h"
#include "Events/Inspector/DeviceChangedEvent.h"

#include <math.h>

#include <QtCore/QObject>
#include <QtCore/QTimer>

RundownPresetWidget::RundownPresetWidget(const LibraryModel& model, QWidget* parent, const QString& color, bool active,
                                         bool inGroup, bool compactView)
    : QWidget(parent),
      active(active), inGroup(inGroup), compactView(compactView), color(color), model(model)
{
    setupUi(this);

    this->animation = new ActiveAnimation(this->labelActiveColor);

    setColor(color);
    setActive(active);
    setCompactView(compactView);

    this->labelGroupColor->setVisible(this->inGroup);
    this->labelGroupColor->setStyleSheet(QString("background-color: %1;").arg(Color::DEFAULT_GROUP_COLOR));
    this->labelColor->setStyleSheet(QString("background-color: %1;").arg(Color::DEFAULT_TRICASTER_COLOR));

    this->labelLabel->setText(this->model.getLabel());
    this->labelDelay->setText(QString("Delay: %1").arg(this->command.getDelay()));
    this->labelDevice->setText(QString("Server: %1").arg(this->model.getDeviceName()));

    this->executeTimer.setSingleShot(true);
    QObject::connect(&this->executeTimer, SIGNAL(timeout()), SLOT(executePlay()));

    QObject::connect(&this->command, SIGNAL(delayChanged(int)), this, SLOT(delayChanged(int)));
    QObject::connect(&this->command, SIGNAL(allowGpiChanged(bool)), this, SLOT(allowGpiChanged(bool)));

    QObject::connect(&TriCasterDeviceManager::getInstance(), SIGNAL(deviceAdded(TriCasterDevice&)), this, SLOT(deviceAdded(TriCasterDevice&)));
    const QSharedPointer<TriCasterDevice> device = TriCasterDeviceManager::getInstance().getDeviceByName(this->model.getDeviceName());
    if (device != NULL)
        QObject::connect(device.data(), SIGNAL(connectionStateChanged(TriCasterDevice&)), this, SLOT(deviceConnectionStateChanged(TriCasterDevice&)));

    QObject::connect(GpiManager::getInstance().getGpiDevice().data(), SIGNAL(connectionStateChanged(bool, GpiDevice*)), this, SLOT(gpiConnectionStateChanged(bool, GpiDevice*)));

    checkEmptyDevice();
    checkGpiConnection();
    checkDeviceConnection();

    qApp->installEventFilter(this);
}

bool RundownPresetWidget::eventFilter(QObject* target, QEvent* event)
{
    if (event->type() == static_cast<QEvent::Type>(Event::EventType::Preview))
    {
        // This event is not for us.
        if (!this->active)
            return false;

        executePlay();

        return true;
    }
    else if (event->type() == static_cast<QEvent::Type>(Event::EventType::LabelChanged))
    {
        // This event is not for us.
        if (!this->active)
            return false;

        LabelChangedEvent* labelChanged = dynamic_cast<LabelChangedEvent*>(event);
        this->model.setLabel(labelChanged->getLabel());

        this->labelLabel->setText(this->model.getLabel());

        return true;
    }
    else if (event->type() == static_cast<QEvent::Type>(Event::EventType::DeviceChanged))
    {
        // This event is not for us.
        if (!this->active)
            return false;

        // Should we update the device name?
        DeviceChangedEvent* deviceChangedEvent = dynamic_cast<DeviceChangedEvent*>(event);
        if (!deviceChangedEvent->getDeviceName().isEmpty() && deviceChangedEvent->getDeviceName() != this->model.getDeviceName())
        {
            // Disconnect connectionStateChanged() from the old device.
            const QSharedPointer<TriCasterDevice> oldDevice = TriCasterDeviceManager::getInstance().getDeviceByName(this->model.getDeviceName());
            if (oldDevice != NULL)
                QObject::disconnect(oldDevice.data(), SIGNAL(connectionStateChanged(TriCasterDevice&)), this, SLOT(deviceConnectionStateChanged(TriCasterDevice&)));

            // Update the model with the new device.
            this->model.setDeviceName(deviceChangedEvent->getDeviceName());
            this->labelDevice->setText(QString("Server: %1").arg(this->model.getDeviceName()));

            // Connect connectionStateChanged() to the new device.
            const QSharedPointer<TriCasterDevice> newDevice = TriCasterDeviceManager::getInstance().getDeviceByName(this->model.getDeviceName());
            if (newDevice != NULL)
                QObject::connect(newDevice.data(), SIGNAL(connectionStateChanged(TriCasterDevice&)), this, SLOT(deviceConnectionStateChanged(TriCasterDevice&)));
        }

        checkEmptyDevice();
        checkDeviceConnection();
    }

    return QObject::eventFilter(target, event);
}

AbstractRundownWidget* RundownPresetWidget::clone()
{
    RundownPresetWidget* widget = new RundownPresetWidget(this->model, this->parentWidget(), this->color, this->active,
                                                          this->inGroup, this->compactView);

    PresetCommand* command = dynamic_cast<PresetCommand*>(widget->getCommand());
    command->setDelay(this->command.getDelay());
    command->setAllowGpi(this->command.getAllowGpi());
    command->setSource(this->command.getSource());
    command->setPreset(this->command.getPreset());

    return widget;
}

void RundownPresetWidget::setCompactView(bool compactView)
{
    if (compactView)
    {
        this->labelIcon->setFixedSize(Rundown::COMPACT_ICON_WIDTH, Rundown::COMPACT_ICON_HEIGHT);
        this->labelGpiConnected->setFixedSize(Rundown::COMPACT_ICON_WIDTH, Rundown::COMPACT_ICON_HEIGHT);
        this->labelDisconnected->setFixedSize(Rundown::COMPACT_ICON_WIDTH, Rundown::COMPACT_ICON_HEIGHT);
    }
    else
    {
        this->labelIcon->setFixedSize(Rundown::DEFAULT_ICON_WIDTH, Rundown::DEFAULT_ICON_HEIGHT);
        this->labelGpiConnected->setFixedSize(Rundown::DEFAULT_ICON_WIDTH, Rundown::DEFAULT_ICON_HEIGHT);
        this->labelDisconnected->setFixedSize(Rundown::DEFAULT_ICON_WIDTH, Rundown::DEFAULT_ICON_HEIGHT);
    }

    this->compactView = compactView;
}

void RundownPresetWidget::readProperties(boost::property_tree::wptree& pt)
{
    if (pt.count(L"color") > 0) setColor(QString::fromStdWString(pt.get<std::wstring>(L"color")));
}

void RundownPresetWidget::writeProperties(QXmlStreamWriter* writer)
{
    writer->writeTextElement("color", this->color);
}

bool RundownPresetWidget::isGroup() const
{
    return false;
}

bool RundownPresetWidget::isInGroup() const
{
    return this->inGroup;
}

AbstractCommand* RundownPresetWidget::getCommand()
{
    return &this->command;
}

LibraryModel* RundownPresetWidget::getLibraryModel()
{
    return &this->model;
}

void RundownPresetWidget::setActive(bool active)
{
    this->active = active;

    this->animation->stop();

    if (this->active)
        this->labelActiveColor->setStyleSheet("background-color: lime;");
    else
        this->labelActiveColor->setStyleSheet("");
}

void RundownPresetWidget::setInGroup(bool inGroup)
{
    this->inGroup = inGroup;
    this->labelGroupColor->setVisible(this->inGroup);
}

void RundownPresetWidget::setColor(const QString& color)
{
    this->color = color;
    this->setStyleSheet(QString("#frameItem, #frameStatus { background-color: rgba(%1); }").arg(color));
}

void RundownPresetWidget::checkEmptyDevice()
{
    if (this->labelDevice->text() == "Device: ")
        this->labelDevice->setStyleSheet("color: black;");
    else
        this->labelDevice->setStyleSheet("");
}

bool RundownPresetWidget::executeCommand(Playout::PlayoutType::Type type)
{
    if (type == Playout::PlayoutType::Play || type == Playout::PlayoutType::Update)
    {       
        if (!this->model.getDeviceName().isEmpty()) // The user need to select a device.
            QTimer::singleShot(this->command.getDelay(), this, SLOT(executePlay()));
    }

    if (this->active)
        this->animation->start(1);

    return true;
}

void RundownPresetWidget::executePlay()
{
    foreach (const TriCasterDeviceModel& model, TriCasterDeviceManager::getInstance().getDeviceModels())
    {
        const QSharedPointer<TriCasterDevice>  device = TriCasterDeviceManager::getInstance().getDeviceByName(model.getName());
        if (device != NULL && device->isConnected())
            device->selectPreset(this->command.getSource(), this->command.getPreset());
    }
}

void RundownPresetWidget::delayChanged(int delay)
{
    this->labelDelay->setText(QString("Delay: %1").arg(delay));
}

void RundownPresetWidget::checkGpiConnection()
{
    this->labelGpiConnected->setVisible(this->command.getAllowGpi());

    if (GpiManager::getInstance().getGpiDevice()->isConnected())
        this->labelGpiConnected->setPixmap(QPixmap(":/Graphics/Images/GpiConnected.png"));
    else
        this->labelGpiConnected->setPixmap(QPixmap(":/Graphics/Images/GpiDisconnected.png"));
}

void RundownPresetWidget::checkDeviceConnection()
{
    const QSharedPointer<TriCasterDevice> device = TriCasterDeviceManager::getInstance().getDeviceByName(this->model.getDeviceName());
    if (device == NULL)
        this->labelDisconnected->setVisible(true);
    else
        this->labelDisconnected->setVisible(!device->isConnected());
}

void RundownPresetWidget::allowGpiChanged(bool allowGpi)
{
    checkGpiConnection();
}

void RundownPresetWidget::gpiConnectionStateChanged(bool connected, GpiDevice* device)
{
    checkGpiConnection();
}

void RundownPresetWidget::deviceConnectionStateChanged(TriCasterDevice& device)
{
    checkDeviceConnection();
}

void RundownPresetWidget::deviceAdded(TriCasterDevice& device)
{
    if (TriCasterDeviceManager::getInstance().getDeviceModelByAddress(device.getAddress()).getName() == this->model.getDeviceName())
        QObject::connect(&device, SIGNAL(connectionStateChanged(TriCasterDevice&)), this, SLOT(deviceConnectionStateChanged(TriCasterDevice&)));

    checkDeviceConnection();
}
