//
// Copyright (c) 2007-2019, Scientific Toolworks, Inc.
//
// This file contains proprietary information of Scientific Toolworks, Inc.
// and is protected by federal copyright law. It may not be copied or
// distributed in any form or medium without prior written authorization
// from Scientific Toolworks, Inc.
//
// Author: Robert Shurtliff
//

#ifndef NEWWIZARD_H
#define NEWWIZARD_H

#include <QWizard>
#include <QSettings>
#include <QTemporaryFile>
#include <QResizeEvent>
#include "api/settings/files.h"
#include "api/settings/file_types.h"
#include "NewWizardSearch.h"
#include "buildstalker/BuildStalker.h"

class QCheckBox;
class QFormLayout;
class QLabel;
class QLineEdit;
class QListWidget;
class QProgressDialog;
class QRadioButton;
class QToolButton;
class QTreeWidget;
class QComboBox;
class QSettings;
class QTemporaryFile;
class QListWidgetItem;

namespace STI {
namespace Configure {

class NewWizard : public QWizard
{
  Q_OBJECT

public:
  enum { Page_SourceCode, 
         Page_ProjectFile, 
         Page_MissingCMakeImport, 
         Page_Import, 
         Page_ImportVisualStudioDetails, 
         Page_ImportJsonDetails,
         Page_WatchBuild, 
         Page_StrictDetails,
         Page_Conclusion };

  enum ResultStatus
  {
    NewWizard_OPEN,
    NewWizard_OPENPARSE,
    NewWizard_FAILURE,
    NewWizard_CANCEL,
    NewWizard_PARSEREQUESTED,
    NewWizard_OPENCONFIGUREREQUESTED,
    NewWizard_CONFIGUREREQUESTED
  };
  
  enum PathTaken
  {
    Path_Standard,
    Path_ImportVisualStudio,
    Path_ImportCMake
  };

  NewWizard(QWidget *parent = 0);

  QStringList mDirectoryList;
  QStringList mImportList;
  QMap<QString, int> mSearchContents;
  QMap<QString, int> mImportSearchContents;
  QMap<QString, STI::Settings::FileTypes::Type> mFileTypes;
  QMap<Settings::Languages::Language, int> mSearchList;
  QListWidget * mLanguageOptionsListWidget;
  
  QStringList mExistingUdbList;
  QStringList mExistingJsonList;
  QStringList mExistingVisualStudioList;
  QStringList mTypeList;
  QStringList mFileExtensions;
  bool mHasCPPFiles;
  bool mHasImportCPPFiles;
  bool mHasCMakeFiles;
  bool mInitializeOptions;
  
  ResultStatus mResultStatus = NewWizard_CANCEL;
  PathTaken mPathTaken = Path_Standard;
  QString mProjectName;
  QString mImportName;
  
  QListWidget * mSourceCodeListWidget;
  QCheckBox * mVisualStudioOneTimeImport;
  QCheckBox * mCMakeOneTimeImport;
  QRadioButton * mStrictRadioButton;
  QRadioButton * mFuzzyRadioButton;
  QCheckBox * mOpenAdvancedSettingsCheckbox;
  QLineEdit * mProjectNameLineEdit;
  QComboBox * mConfigurationComboBox;
  
  QComboBox * mAdaVersionComboBox;
  QComboBox * mAssemblyAssemblerComboBox;
  QComboBox * mCobolCompilerComboBox;
  QComboBox * mCobolFormatComboBox;
  QComboBox * mCppCompilerComboBox;
  QComboBox * mCppStrictCompilerComboBox;
  QComboBox * mFortranVersionComboBox;
  QComboBox * mFortranFormatComboBox;
  QComboBox * mJavaVersionComboBox;
  QComboBox * mJovialVersionComboBox;
  QComboBox * mPascalVersionComboBox;
  QComboBox * mPlmVersionComboBox;
  QComboBox * mPythonVersionComboBox;
  QComboBox * mWebVersionComboBox;

  void accept();
  QProgressDialog * mProgressDialog;
  bool mBreakout = false;
  int mNumberOfAddedFiles = 0;
  int mNumberOfToBeAddedFiles = 0;
  int mNumberOfAddedDirectories = 0;
  QString mFilesToBeAddedString;
  
  QTemporaryFile mComboboxDownArrow;

  NewWizardSearch * mNewWizardSearch;

public slots:
  void openProjectWizard();
  void clearAllSettings();
  void idChanged(int id);
  void showHelp();
  void updateProgressDialog();
  void changeProgressDialog();
  
private slots:
  void cancelWizard();
  void initOptions();
};

class SourceCodePage : public QWizardPage
{
  Q_OBJECT

public:
  SourceCodePage(NewWizard * wizardParent);

  int nextId() const override;
  bool validatePage() override;
  void initializePage() override;

private:
  QLabel *topLabel;
  
  QToolButton * sourceCodeAddDirectoryButton;
  
  NewWizard * mParentWizard;
  QProgressDialog * mProgressDialog;

};

class ProjectFilePage : public QWizardPage
{
  Q_OBJECT

public:
  ProjectFilePage(NewWizard * wizardParent);

  int nextId() const override;
  bool validatePage() override;
  void initializePage() override;
  
  virtual void resizeEvent(QResizeEvent *event) override;

private:
  void HandleSelectionChanged(QListWidgetItem *current,QListWidgetItem *previous);

  QListWidget * projectFileListWidget;
  NewWizard * mParentWizard;
  QLabel *topLabel;
  int projectButtonWidth = 60;
};

class MissingCMakeImportPage : public QWizardPage
{
  Q_OBJECT

public:
  MissingCMakeImportPage(NewWizard * wizardParent);

  int nextId() const override;
  bool validatePage() override;
  void initializePage() override;
  void importProjectWizard();

private:
  QLabel *topLabel;
  
  QToolButton * missingCMakeAddFileButton;
  
  //QListWidget * missingCMakeListWidget;
  
  QToolButton * importButton;
  NewWizard * mParentWizard;
  bool importButtonPressed = false;
  QLabel * bottomLabel;
  QLabel * bottomInstructions;
};

class ImportPage : public QWizardPage
{
  Q_OBJECT

public:
  ImportPage(NewWizard * wizardParent);

  void initializePage() override;
  int nextId() const override;
  void importProjectWizard();
  
  virtual void resizeEvent(QResizeEvent *event) override;

private:
  void HandleSelectionChanged(QListWidgetItem *current,QListWidgetItem *previous);
  
  NewWizard * mParentWizard;
  QListWidget * importListWidget;
  bool importButtonPressed = false;
  QLabel *topLabel;
};

//Put new page here
class WatchBuildPage : public QWizardPage
{
  Q_OBJECT

public:
  WatchBuildPage(NewWizard * wizardParent);

  int nextId() const override;
  //bool validatePage() override;
  void initializePage() override;

private:
  QLabel *topLabel;
  QPushButton * mWatchButton;
  NewWizard * mParentWizard;
  //bool * watching;
  BuildStalker * bs; 
  QListWidget * mListWidget;
  QLabel *loaderLabel;
  QMovie *loader;
  QProgressDialog * mProgressDialog;
};
//

class ImportJsonDetailsPage : public QWizardPage
{
  Q_OBJECT

public:
  ImportJsonDetailsPage(NewWizard * wizardParent);
  int nextId() const override;
  void initializePage() override;
  bool validatePage() override;

  
private:
  
  void UpdateCMakeList (
    const QString &text);

  NewWizard * mParentWizard;

  QLabel * mTopLabel;
  QFormLayout * mFormLayout;
  QListWidget * mCMakeListWidget;
  QLabel *bottomLabel;
};

class ImportVisualStudioDetailsPage : public QWizardPage
{
  Q_OBJECT

public:
  ImportVisualStudioDetailsPage(NewWizard * wizardParent);

  int nextId() const override;
  void initializePage() override;
  bool validatePage() override;

private slots:
  void CurrentIndexChanged (
    const QString &text);
  
private:
  
  void CurrentChanged();
  void ConfigurationTextChanged (
    const QString &text);
  void AddProjectFile(
    const QString &project,
    const QString &solution,
    const QString &config);
  void AddFile(
    const QString &filename,
    const QString &config);

  NewWizard * mParentWizard;

  QFormLayout * mFormLayout;
  QTreeWidget * mVisualStudioTreeWidget;
  QLabel *topLabel;
  QLabel *bottomLabel;
};

class StrictDetailsPage : public QWizardPage
{
  Q_OBJECT

public:
  StrictDetailsPage(NewWizard * wizardParent);

  int nextId() const override;
  void initializePage() override;
private:

  NewWizard * mParentWizard;
};

class ConclusionPage : public QWizardPage
{
  Q_OBJECT

public:
  ConclusionPage(NewWizard * wizardParent);

  void initializePage() override;
  int nextId() const override;

private:
  QListWidget * extentionListWidget;
  QToolButton * conclusionAddFilenameButton;

  NewWizard * mParentWizard;
};

// A message class needed for adding files (specifically, the matchFiles call)
class NewWizardMessage : public STI::Settings::Files::Msg {
public:
  //*********************************
  // Overridden Functions
  //*********************************
  NewWizardMessage(NewWizard * wizardParent);
  bool CheckDirectory(const QString & name, const bool topLevel);
  void DeleteFile(const QString & name);
  void MatchFile(const QString & name);
  void AddFile(STI::Settings::AutoFile * autofile);
  void AddFilesFound(int filesFound);

  // Other Functions
  QStringList & getMatchedFiles();
  int getNumAdded();
  int getNumRemoved();
  void TestAbort();

private:
  QStringList mMatchedFiles;
  int mNumAdded = 0;
  int mNumRemoved = 0;
  NewWizard * mParentWizard;

};
}
}
#endif
