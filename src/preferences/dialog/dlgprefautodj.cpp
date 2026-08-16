#include "preferences/dialog/dlgprefautodj.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QSpinBox>

#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
#include <QTimeZone>
#endif

#include <cmath>

#include "library/library.h"
#include "library/trackcollection.h"
#include "library/trackcollectionmanager.h"
#include "library/trackset/crate/crate.h"
#include "library/trackset/crate/cratestorage.h"
#include "moc_dlgprefautodj.cpp"

DlgPrefAutoDJ::DlgPrefAutoDJ(QWidget* pParent,
        UserSettingsPointer pConfig,
        std::shared_ptr<Library> pLibrary)
        : DlgPreferencePage(pParent),
          m_pConfig(pConfig),
          m_pLibrary(std::move(pLibrary)),
          m_pWeightContainer(nullptr) {
    setupUi(this);

    // The auto-DJ replay-age for randomly-selected tracks
    connect(RequeueIgnoreCheckBox,
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
            &QCheckBox::checkStateChanged,
#else
            &QCheckBox::stateChanged,
#endif
            this,
            &DlgPrefAutoDJ::slotToggleRequeueIgnore);

    // Auto DJ random enqueue
    connect(RandomQueueCheckBox,
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
            &QCheckBox::checkStateChanged,
#else
            &QCheckBox::stateChanged,
#endif
            this,
            &DlgPrefAutoDJ::slotToggleRandomQueue);

    setScrollSafeGuardForAllInputWidgets(this);
}

void DlgPrefAutoDJ::slotUpdate() {
    // The minimum available for randomly-selected tracks
    MinimumAvailableSpinBox->setValue(
            m_pConfig->getValue(
                    ConfigKey("[Auto DJ]", "MinimumAvailable"), 20));

    // The auto-DJ replay-age for randomly-selected tracks
    RequeueIgnoreCheckBox->setChecked(m_pConfig->getValue(
            ConfigKey("[Auto DJ]", "UseIgnoreTime"), false));
    /// TODO: Once we require at least Qt 6.7, remove this `setTimeZone` call
    /// and uncomment the corresponding declarations in the UI file instead.
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
    RequeueIgnoreTimeEdit->setTimeZone(QTimeZone::LocalTime);
#else
    RequeueIgnoreTimeEdit->setTimeSpec(Qt::LocalTime);
#endif
    RequeueIgnoreTimeEdit->setTime(
            QTime::fromString(
                    m_pConfig->getValue(
                            ConfigKey("[Auto DJ]", "IgnoreTime"), "23:59"),
                    RequeueIgnoreTimeEdit->displayFormat()));
    RequeueIgnoreTimeEdit->setEnabled(
            RequeueIgnoreCheckBox->checkState() == Qt::Checked);

    // Auto DJ random enqueue
    RandomQueueCheckBox->setChecked(m_pConfig->getValue(
            ConfigKey("[Auto DJ]", "EnableRandomQueue"), false));

    RandomQueueMinimumSpinBox->setValue(
            m_pConfig->getValue(
                    ConfigKey("[Auto DJ]", "RandomQueueMinimumAllowed"), 5));
    // "[Auto DJ], Requeue" is set by 'Repeat Playlist' toggle in DlgAutoDj GUI.
    // If it's checked un-check 'Random Queue'
    // TODO Add 'Repeat' checkbox here, or add a hint why the checkbox may be disabled
    considerRepeatPlaylistState(
            m_pConfig->getValue<bool>(ConfigKey("[Auto DJ]", "Requeue")));
    slotToggleRandomQueue(
            m_pConfig->getValue<bool>(
                    ConfigKey("[Auto DJ]", "EnableRandomQueue"))
                    ? Qt::Checked
                    : Qt::Unchecked);

    // Re-center the crossfader instantly when AutoDJ is disabled
    CenterXfaderCheckBox->setChecked(m_pConfig->getValue(
            ConfigKey("[Auto DJ]", "center_xfader_when_disabling"), false));

    // --- Crate Weights UI ---
    // Clear existing dynamic widgets
    m_weightSpinners.clear();
    m_percentLabels.clear();
    if (m_pWeightContainer) {
        delete m_pWeightContainer;
        m_pWeightContainer = nullptr;
    }

    // Query all AutoDJ source crates
    const CrateStorage& crateStorage =
            m_pLibrary->trackCollectionManager()->internalCollection()->crates();
    CrateSelectResult autoDjCrates(crateStorage.selectAutoDjCrates(true));

    struct CrateInfo {
        CrateId id;
        QString name;
        int weight;
    };
    QVector<CrateInfo> crateList;
    Crate crate;
    while (autoDjCrates.populateNext(&crate)) {
        crateList.append({crate.getId(), crate.getName(), crate.autoDjWeight()});
    }

    if (crateList.isEmpty()) {
        // Show "no crates" message
        labelNoCrates->setVisible(true);
        scrollAreaWeights->setVisible(false);
        return;
    }

    labelNoCrates->setVisible(false);
    scrollAreaWeights->setVisible(true);

    // Create a container widget for the scroll area
    m_pWeightContainer = new QWidget();
    QVBoxLayout* pContainerLayout = new QVBoxLayout(m_pWeightContainer);

    for (const auto& crateInfo : crateList) {
        QHBoxLayout* pRowLayout = new QHBoxLayout();

        // Crate name label
        QLabel* pNameLabel = new QLabel(crateInfo.name, m_pWeightContainer);
        pNameLabel->setMinimumWidth(120);
        pRowLayout->addWidget(pNameLabel);

        // Weight spinner
        QSpinBox* pSpinner = new QSpinBox(m_pWeightContainer);
        pSpinner->setMinimum(1);
        pSpinner->setMaximum(100);
        pSpinner->setSingleStep(1);
        pSpinner->setValue(crateInfo.weight);
        pSpinner->setMinimumWidth(60);
        pSpinner->setMaximumWidth(80);
        pRowLayout->addWidget(pSpinner);

        // Percentage label
        QLabel* pPercentLabel = new QLabel(m_pWeightContainer);
        pPercentLabel->setMinimumWidth(40);
        pRowLayout->addWidget(pPercentLabel);

        pRowLayout->addStretch();

        pContainerLayout->addLayout(pRowLayout);

        m_weightSpinners.append(qMakePair(crateInfo.id, pSpinner));
        m_percentLabels.append(pPercentLabel);

        connect(pSpinner,
                QOverload<int>::of(&QSpinBox::valueChanged),
                this,
                &DlgPrefAutoDJ::slotWeightChanged);
    }

    pContainerLayout->addStretch();

    scrollAreaWeights->setWidget(m_pWeightContainer);

    // Calculate initial percentages
    slotWeightChanged();
}

void DlgPrefAutoDJ::slotApply() {
    m_pConfig->setValue(ConfigKey("[Auto DJ]", "MinimumAvailable"),
            MinimumAvailableSpinBox->value());

    m_pConfig->setValue(ConfigKey("[Auto DJ]", "UseIgnoreTime"),
            RequeueIgnoreCheckBox->isChecked());
    const QString ignTimeStr =
            RequeueIgnoreTimeEdit->time().toString();
    m_pConfig->setValue(ConfigKey("[Auto DJ]", "IgnoreTime"), ignTimeStr);

    m_pConfig->setValue(ConfigKey("[Auto DJ]", "EnableRandomQueue"),
            RandomQueueCheckBox->isChecked());
    m_pConfig->setValue(
            ConfigKey("[Auto DJ]", "RandomQueueMinimumAllowed"),
            RandomQueueMinimumSpinBox->value());

    m_pConfig->setValue(ConfigKey("[Auto DJ]", "center_xfader_when_disabling"),
            CenterXfaderCheckBox->isChecked());

    // Persist crate weights
    TrackCollection* pTrackCollection =
            m_pLibrary->trackCollectionManager()->internalCollection();
    for (const auto& pair : m_weightSpinners) {
        const CrateId crateId = pair.first;
        QSpinBox* pSpinner = pair.second;
        if (!pSpinner) {
            continue;
        }

        Crate crate;
        if (!pTrackCollection->crates().readCrateById(crateId, &crate)) {
            continue;
        }

        crate.setAutoDjWeight(pSpinner->value());
        if (!pTrackCollection->updateCrate(crate)) {
            QMessageBox::warning(this,
                    tr("Auto DJ Weight Error"),
                    tr("Failed to save the weight for crate \"%1\". "
                       "Please try again.")
                            .arg(crate.getName()));
            return;
        }
    }
}

void DlgPrefAutoDJ::slotResetToDefaults() {
    MinimumAvailableSpinBox->setValue(20);

    RequeueIgnoreCheckBox->setChecked(false);
    RequeueIgnoreTimeEdit->setEnabled(false);
    RequeueIgnoreTimeEdit->setTime(QTime::fromString("23:59"));

    RandomQueueCheckBox->setChecked(false);
    RandomQueueCheckBox->setEnabled(true);
    RandomQueueMinimumSpinBox->setEnabled(false);
    RandomQueueMinimumSpinBox->setValue(5);

    CenterXfaderCheckBox->setChecked(false);

    // Reset all weight spinners to default value of 10
    for (const auto& pair : m_weightSpinners) {
        QSpinBox* pSpinner = pair.second;
        if (pSpinner) {
            pSpinner->setValue(10);
        }
    }
    slotWeightChanged();
}

void DlgPrefAutoDJ::slotWeightChanged() {
    // Recalculate all percentage labels based on current spinner values
    int sum = 0;
    for (const auto& pair : m_weightSpinners) {
        QSpinBox* pSpinner = pair.second;
        if (pSpinner) {
            sum += pSpinner->value();
        }
    }

    if (sum == 0) {
        return;
    }

    for (int i = 0; i < m_weightSpinners.size() && i < m_percentLabels.size(); ++i) {
        QSpinBox* pSpinner = m_weightSpinners[i].second;
        QLabel* pLabel = m_percentLabels[i];
        if (pSpinner && pLabel) {
            int percent = static_cast<int>(
                    std::round(static_cast<double>(pSpinner->value()) / sum * 100.0));
            pLabel->setText(QStringLiteral("%1%").arg(percent));
        }
    }
}

#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
void DlgPrefAutoDJ::slotToggleRequeueIgnore(Qt::CheckState buttonState) {
#else
void DlgPrefAutoDJ::slotToggleRequeueIgnore(int buttonState) {
#endif
    RequeueIgnoreTimeEdit->setEnabled(buttonState == Qt::Checked);
}

void DlgPrefAutoDJ::considerRepeatPlaylistState(bool enable) {
    RandomQueueMinimumSpinBox->setEnabled(enable);
}

#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
void DlgPrefAutoDJ::slotToggleRandomQueue(Qt::CheckState buttonState) {
#else
void DlgPrefAutoDJ::slotToggleRandomQueue(int buttonState) {
#endif
    RandomQueueMinimumSpinBox->setEnabled(buttonState == Qt::Checked);
}
