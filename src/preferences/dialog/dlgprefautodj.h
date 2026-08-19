#pragma once

#include <QPair>
#include <QVector>
#include <memory>

#include "library/trackset/crate/crateid.h"
#include "preferences/dialog/dlgpreferencepage.h"
#include "preferences/dialog/ui_dlgprefautodjdlg.h"
#include "preferences/usersettings.h"

class Library;
class QLabel;
class QSpinBox;
class QWidget;

class DlgPrefAutoDJ : public DlgPreferencePage, public Ui::DlgPrefAutoDJDlg {
    Q_OBJECT
  public:
    DlgPrefAutoDJ(QWidget* pParent,
            UserSettingsPointer pConfig,
            std::shared_ptr<Library> pLibrary);

  public slots:
    void slotUpdate() override;
    void slotApply() override;
    void slotResetToDefaults() override;

  private slots:
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
    void slotToggleRequeueIgnore(Qt::CheckState state);
#else
    void slotToggleRequeueIgnore(int buttonState);
#endif
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
    void slotToggleRandomQueue(Qt::CheckState state);
#else
    void slotToggleRandomQueue(int buttonState);
#endif
    void slotWeightChanged();

  private:
    void considerRepeatPlaylistState(bool);

    UserSettingsPointer m_pConfig;
    std::shared_ptr<Library> m_pLibrary;

    QWidget* m_pWeightContainer;
    QVector<QPair<CrateId, QSpinBox*>> m_weightSpinners;
    QVector<QLabel*> m_percentLabels;
};
