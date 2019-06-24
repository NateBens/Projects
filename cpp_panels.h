//
// Copyright (c) 2006-2018, Scientific Toolworks, Inc.
//
// This file contains proprietary information of Scientific Toolworks, Inc.
// and is protected by federal copyright law. It may not be copied or
// distributed in any form or medium without prior written authorization
// from Scientific Toolworks, Inc.
//
// Author: Roy Moffitt
//

//cpp optons panels for setting project wide settings specific to c/cpp language
//api/project/cpp_options.h interface

#ifndef CPP_PANELS_H
#define CPP_PANELS_H

#include "macrospanel.h"
#include "includespanel.h"
#include "api/settings/values.h"


namespace STI {
  namespace Settings {
    class CppOptions;
    class File;
  }
}

class QString;
class QWidget;
class QLabel;
class QComboBox;
class QPushButton;
class QLineEdit;
class QResizeEvent;
class QCheckBox;
class QSpinBox;


namespace STI {
namespace Configure {

class CppMacrosPanel : public MacrosPanel {
  Q_OBJECT
private:
  Settings::CppOptions *cppoptions;
public:
  CppMacrosPanel(QWidget *parent,Settings::CppOptions *,Settings::ValuesRef,ManagedDb::DatabaseRef senddb); // parent optional, options required
  void UpdateOptions(void); // called to update AdaOptions on every alter
  void OnAdd(void) {
    UpdateOptions();
  }; // save current to project memory
  void OnRemove(void) {
    UpdateOptions();
  }; // save current to project memory

  bool isValid(QString macro);
  ManagedDb::DatabaseRef db;
  Settings::ValuesRef proj;
  ManagedDb::EntityList mMacros;
  void OnUndefined();

public slots:
  void openCppMissingMacros();
};


class CppFileMacrosPanel : public MacrosPanel {
  Q_OBJECT
private:
  Settings::File *filedata;
  QCheckBox *ignoreOverride_chk;

private slots:
  void onStateChangedIgnoreOverride(int);

public:
  CppFileMacrosPanel(QWidget *parent,Settings::File *); // parent optional, file required
  void UpdateOptions(); // called to update AdaOptions on every alter
  void OnAdd() {
    UpdateOptions();
  }; // save current to project memory
  void OnRemove(void) {
    UpdateOptions();
  }; // save current to project memory
  bool isValid(QString macro);
  bool isDefault();

protected:
  void resizeEvent ( QResizeEvent * event );
};


class CppCompilerPanel: public QWidget {
  Q_OBJECT

private slots:
  void onCompilerChanged(int);
  void onIncludePathChanged();
  void onAllowNestedChanged(int);

private:
  Settings::CppOptions *cppoptions;

public:
  CppCompilerPanel(QWidget *parent,Settings::CppOptions *);   // parent optional, options required
  bool altered;
  QLabel *compilerlabel;
  QComboBox *compilerchoice;
  QLabel *includepathslabel;
  QLineEdit *inlcudepathstext;
  QCheckBox *allownestedcheck;
  void PositionWidgets();

protected:
  void resizeEvent ( QResizeEvent * event );
};


class CppIncludesPanel : public IncludesPanel { // old understand ml database
  Q_OBJECT

private slots:
  void onAfiChanged(int);
  void onAfsChanged(int);
  void onPmiChanged(int);
  void onSfiChanged(int);
  void onTsiChanged(int);
  void onUsiChanged(int);
  void onIdiChanged(int);

private:
  Settings::CppOptions *cppoptions;

public:
  CppIncludesPanel(QWidget *parent,QString title,Settings::CppOptions *passedcppoptions,ManagedDb::DatabaseRef senddb);
  void UpdateOptions();
  void OnAlter(void) {
    UpdateOptions();
  }; // save current to project memory


  QCheckBox *aficheck; // Add Found Include Files To Source List
  QCheckBox *afscheck; // Add Found System Include Files To Source List
  QCheckBox *pmicheck; // Prompt For Missing Include Files
  QCheckBox *sficheck; // Search For Include Files Among Project Files
  QCheckBox *tsicheck; // Treat System Includes As User Includes
  QCheckBox *usicheck; // Use Case-Insensitive Lookup For Includes [UNIX ONLY]
  QCheckBox *idicheck; // Ignore include dir when looking for includes

public slots:
  void reloadIncludes();
  void openCppSearch(QStringList missingFiles);
};


class CppFileIncludesPanel : public IncludesPanel {
  Q_OBJECT

private:
  Settings::File *filedata;
  QCheckBox *ignoreOverride_chk;

private slots:
  void onStateChangedIgnoreOverride(int);

public:
  CppFileIncludesPanel(QWidget *parent,QString title,Settings::File *passedfiledata,ManagedDb::DatabaseRef senddb);
  void UpdateOptions();
  void OnAlter(void) {
    UpdateOptions();
  }; // save current to project memory
  bool isDefault();
};


class CppFileAutoIncludesPanel : public IncludesPanel {
  Q_OBJECT

private:
  Settings::File *filedata;
  QCheckBox *ignoreOverride_chk;

private slots:
  void onStateChangedIgnoreOverride(int);

public:
  CppFileAutoIncludesPanel(QWidget *parent,QString title,Settings::File *passedfiledata,ManagedDb::DatabaseRef senddb);
  void UpdateOptions();
  void OnAlter(void) {
    UpdateOptions();
  }; // save current to project memory
  void OnNew();
  bool isDefault();
};


class CppAutoIncludesPanel : public IncludesPanel {
  Q_OBJECT

private:
  Settings::CppOptions *cppoptions;

public:
  CppAutoIncludesPanel(QWidget *parent,QString title,Settings::CppOptions *,ManagedDb::DatabaseRef senddb);
  void UpdateOptions();
  void OnAlter(void) {
    UpdateOptions();
  }; // save current to project memory
  void OnNew();

public slots:
  void reloadAutoIncludes();
};


class CppIgnoreIncludesPanel : public IncludesPanel {
  Q_OBJECT

private:
  Settings::CppOptions *cppoptions;

public:
  CppIgnoreIncludesPanel(QWidget *parent,QString title,Settings::CppOptions *passedcppoptions,ManagedDb::DatabaseRef senddb);
  void UpdateOptions();
  void OnAlter(void) {
    UpdateOptions();
  }; // save current to project memory
  void OnNew();
  void OnEdit();
};


class CppReplaceTextPanel : public MacrosPanelOrdered { // replace text for includes
  Q_OBJECT

private:
  Settings::CppOptions *cppoptions;

public:
  CppReplaceTextPanel(QWidget *parent,Settings::CppOptions *); // parent optional, options required
  void UpdateOptions(); // called to update AdaOptions on every alter
  void OnAdd() {
    UpdateOptions();
  }; // save current to project memory
  void OnRemove() {
    UpdateOptions();
  }; // save current to project memory
  virtual void OnNew();
  virtual void OnEdit();
};


class CppReplaceTextConfig : public MacroDefineConfig {
  Q_OBJECT

public:
  CppReplaceTextConfig(QWidget *parent,MacrosPanel *pp,QString *startmacroname,QString *startmacrodef);
};


class CppUndefineMacrosPanel : public IncludesPanel {
  Q_OBJECT

private:
  Settings::CppOptions *cppoptions;

private slots:
  virtual void onImport();

public:
  CppUndefineMacrosPanel(QWidget *parent,QString title,Settings::CppOptions *passedcppoptions,ManagedDb::DatabaseRef senddb);
  void UpdateOptions();
  void OnAlter() {
    UpdateOptions();
  }; // save current to project memory
  void OnNew();
};


// panel for configuring adding a undefine macro
class UndefineMacroPanel : public QWidget {
  Q_OBJECT

private:
  CppUndefineMacrosPanel *assocpanel;

public:
  UndefineMacroPanel(QWidget *parent,CppUndefineMacrosPanel *pp);
  void PositionWidgets();
  void SaveSettings();
  QLabel *mlabel;
  QLineEdit *mtext;

protected:
  void resizeEvent (QResizeEvent *event );
};


class UndefineMacroConfig : public QDialog {
  Q_OBJECT

private slots:
  void onOkay();
  void onCancel();

public:
  UndefineMacroConfig(QWidget *parent,CppUndefineMacrosPanel *pp);
  void PositionWidgets();
  CppUndefineMacrosPanel *msp;
  UndefineMacroPanel *mp;
  QPushButton *okaybutton;
  QPushButton *cancelbutton;

protected:
  void resizeEvent (QResizeEvent *);
};


class CppOptimizePanel: public QWidget {
  Q_OBJECT

private slots:
  void onImplicitChanged(int);
  void onInactiveChanged(int);
  void onLocalChanged(int);
  void onMacrosChanged(int);
  void onParametersChanged(int);
  void onCommentsChanged(int);
  void onDuplicateChanged(int);
  void onExpansionChanged(int);
  void onCacheChanged(int);
  void onAssemblyChanged(int);

private:
  Settings::CppOptions *cppoptions;

public:
  CppOptimizePanel(QWidget *parent,Settings::CppOptions *);   // parent optional, options required
  QCheckBox *Implicitcheck;
  QCheckBox *Inactivecheck;
  QCheckBox *Localcheck;
  QCheckBox *Macroscheck;
  QCheckBox *Parameterscheck;
  QCheckBox *assemblyCheck;
  QCheckBox *Commentscheck;
  QCheckBox *Duplicatecheck;
  QCheckBox *Expansioncheck;
  QCheckBox *Cachecheck;
  void PositionWidgets();

protected:
  void resizeEvent(QResizeEvent *);
};


class CppResolvePanel: public QWidget {
  Q_OBJECT

private slots:
  void onMergeDuplicateFunctionsChanged(int);

private:
  Settings::CppOptions *cppoptions;

public:
  CppResolvePanel(QWidget *parent,Settings::CppOptions *);
  QCheckBox *MergeDuplicateFunctions;
  void PositionWidgets();

protected:
  void resizeEvent(QResizeEvent *);
};


} // Configure
} // STI

#endif
