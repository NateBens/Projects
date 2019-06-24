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

#include <QtWidgets>
#include <QCheckBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QProgressDialog>
#include <QRadioButton>
#include <QToolButton>
#include <QTreeWidget>
#include <QComboBox>
#include <QItemDelegate>

#include "api/cmake/CompileDatabase.h"
#include "api/compilers/Compiler.h"
#include "api/settings/ada_options.h"
#include "api/settings/assembly_options.h"
#include "api/settings/cmake_options.h"
#include "api/settings/cobol_options.h"
#include "api/settings/cpp_options.h"
#include "api/settings/directory.h"
#include "api/settings/file_types.h"
#include "api/settings/files.h"
#include "api/settings/fortran_options.h"
#include "api/settings/java_options.h"
#include "api/settings/jovial_options.h"
#include "api/settings/languages.h"
#include "api/settings/pascal_options.h"
#include "api/settings/plm_options.h"
#include "api/settings/python_options.h"
#include "api/settings/sync_file.h"
#include "api/settings/visual_studio_options.h"
#include "api/settings/web_options.h"
#include "api/tracking/Tracking.h"
#include "api/visualstudio/File.h"
#include "api/visualstudio/Project.h"
#include "api/visualstudio/Solution.h"
//#include "buildstalker/BuildStalker.h"
#include "foundation/FileUtils.h"
#include "maintain_configure_api.h"
#include "maintainlib/ProjectApp.h"
#include "manageddb/Database.h"
#include "manageddb/DatabaseManager.h"
#include "manageddb/Language.h"
#include "manageddb/ManagedDb.h"
#include "NewWizard.h"
#include "sourcepanel.h" //guage
#include "util/theme/Icon.h"
#include "util/theme/Theme.h"
#include "widgets/FilePathWidget.h"
#include "widgets/PersistentMessageBox.h"

namespace STI {
namespace Configure {

class ComboBoxDelegate : public QItemDelegate
{
public:
	ComboBoxDelegate(QWidget *parent = nullptr)
		: QItemDelegate(parent) {
		
	}
	
protected:
    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const;
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const;
};

void ComboBoxDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    if(index.data(Qt::AccessibleDescriptionRole).toString() == QLatin1String("separator"))
    {
        painter->setPen(STI::STIApplication::theme()->group1("accent-1"));
        painter->drawLine(option.rect.left(), option.rect.center().y(), option.rect.right(), option.rect.center().y());
    }
    else
        QItemDelegate::paint(painter, option, index);
}

QSize ComboBoxDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    QString type = index.data(Qt::AccessibleDescriptionRole).toString();
    if(type == QLatin1String("separator"))
        return QSize(0, 2);
    return QItemDelegate::sizeHint( option, index );
}

class ButtonComboBox : public QComboBox
{
public:
  ButtonComboBox(QWidget *parent = nullptr)
    : QComboBox(parent),
      isPopupShown(false),
      timer(new QTimer(this))
  {
    auto lineEdit = new QLineEdit;
    lineEdit->installEventFilter(this);
    setLineEdit(lineEdit);

    timer->setSingleShot(true);
    timer->setInterval(100);
  }

protected:
  bool eventFilter(QObject *object, QEvent *event) override
  {
    if (object == lineEdit() && event->type() == QEvent::MouseButtonRelease) {
      if (!timer->isActive()) {
        if (!isPopupShown)
          showPopup();
        else if (isPopupShown)
          hidePopup();

        QLineEdit * lineEdit = dynamic_cast<QLineEdit*>(object);
        lineEdit->deselect();
        return true;
      }
    }

    return false;
  }

  void showPopup() override
  {
    QComboBox::showPopup();
    isPopupShown = true;
    timer->start();
  }

  void hidePopup() override
  {
    QComboBox::hidePopup();
    isPopupShown = false;
    timer->start();
  }

private:
  bool isPopupShown;
  QTimer *timer;
};

const char *kWinOS = "windows";
const char *kMacOS = "macosx";
const char *kIosOS = "ios";

const QMap<QString,QString> kDefaultCompilers = {
  {"Clang", "Mac"},
  {"GCC", "Linux"},
  {"MSVC", "Microsoft Visual C++"}
};

NewWizard::NewWizard(QWidget *parent)
  : QWizard(parent)
{
  mDirectoryList = QStringList();
  mSearchContents.clear();
  mExistingUdbList.clear();
  mExistingJsonList.clear();
  mExistingVisualStudioList.clear();

  mHasCPPFiles = false;
  mHasImportCPPFiles = false;
  mHasCMakeFiles = false;
  
  // create a temporary file for correct colored image - QCombobox down-arrow
  mComboboxDownArrow.setFileTemplate(QDir::tempPath() + "/XXXXXX.png"); 
  mComboboxDownArrow.open();
  mComboboxDownArrow.close();
  QIcon bOpen(Icon(":/icons/angle-down.svg", STI::STIApplication::theme()->group1("accent-1")));
  QPixmap pixmap = bOpen.pixmap(QSize(32,32));
  pixmap.save(mComboboxDownArrow.fileName());

  mLanguageOptionsListWidget = new QListWidget(this);
  
  mSourceCodeListWidget = new QListWidget(this);
  mSourceCodeListWidget->setSelectionMode(QAbstractItemView::SingleSelection);

  mVisualStudioOneTimeImport = new QCheckBox(tr("One time import, do not synchronize this project."));
  mVisualStudioOneTimeImport->setChecked(false);
  mCMakeOneTimeImport = new QCheckBox(tr("One time import, do not synchronize this project."));
  mCMakeOneTimeImport->setChecked(false);

  mStrictRadioButton = new QRadioButton(tr("Strict"));
  mFuzzyRadioButton = new QRadioButton(tr("Fuzzy"));
  
#ifndef QT_NO_TOOLTIP
  mVisualStudioOneTimeImport->setToolTip(tr("Normally the Understand project stays synchronized with the Visual Studio project, "
                                         "so if a file is added to the MSVC project it will automatically be added to the Understand project. "
                                         "Check this option if you do not want them synchronized. "
                                         "We will still create your initial Understand project with the same settings as the Visual Studio project.", nullptr));
  mCMakeOneTimeImport->setToolTip(tr("Normally the Understand project stays synchronized with the CMake project, "
                                     "so if a file is added to the CMake project it will automatically be added to the Understand project. "
                                     "Check this option if you do not want them synchronized. We will still create your initial Understand project "
                                     "with the same settings as the CMake project.", nullptr));
#endif // QT_NO_TOOLTIP

  mOpenAdvancedSettingsCheckbox = new QCheckBox(tr("Configure Advanced Settings after project creation."));
  mProjectNameLineEdit = new QLineEdit();
  
  mConfigurationComboBox = new ButtonComboBox();
  mConfigurationComboBox->setSizeAdjustPolicy(QComboBox::AdjustToContents);
  mConfigurationComboBox->setEditable(true);
  mConfigurationComboBox->lineEdit()->setReadOnly(true);
  mConfigurationComboBox->view()->setTextElideMode(Qt::ElideNone); 
  mConfigurationComboBox->lineEdit()->setAlignment(Qt::AlignLeft);  
  
  Theme *theme = STI::STIApplication::theme();
  QColor background1 = STI::STIApplication::theme()->group3("toggled");
  QColor foreground1 = STI::STIApplication::theme()->group3("background");
  foreground1.setAlpha(128);
  
  setStyleSheet(QString(".QComboBox{background-color:transparent;"
                "border-color: gray;"
                "border-width: 0px;"
                "border-style: solid;"
                "padding: 1px 0px 0px 2px;"
                "margin: 0 5 0 5;"
                "}"//the padding/border/margin change is what makes the color change work - stupid bug!!!!
                ".QFrame{background-color: %2;}"
                ".QComboBox::drop-down {background-color:transparent;"
                 "width: 15px; height: 15px;"
                "}"
                "QListView[type=\"1\"] {background-color: transparent; qproperty-alternatingRowColors : false;}"
                "QListView[type=\"1\"]::item {background: %5;"
                "border: 2px solid %4;"
                "border-radius: 3px;"
                "}"
                "QListView[type=\"1\"]::item:selected {background: %5;"
                //"border: 2px solid %10;"
                "border: 2px solid %4;"
                "border-radius: 3px;"
                "}"
                ".QComboBox::down-arrow {"
                "image:url(\"%1\");"
                "width: 10px; height: 10px;"
                "}"
                ".QLabel[type=\"1\"] {"
                "font-weight: bold;"
                "background-color: transparent;"
                "}"
                ".QLabel[type=\"2\"] {"
                "color: %7;"
                "background-color: transparent;"
                "}"
                ".QLabel[type=\"3\"] {"
                "color: %10;"
                "background-color: transparent;"
                "}"
                "QToolButton {"
                "margin: 1px;"
                "border-color: %6;"
                "border-style: outset;"
                "border-radius: 3px;"
                "border-width: 1px;"
                "color: %3;"
                "background-color: %4);"
                "}"
                "QToolButton:pressed {"
                "color: %7;"
                "border-color: %10;"
                "background-color: %8;"
                "}").arg(mComboboxDownArrow.fileName())
                .arg(foreground1.name(QColor::HexArgb))
                .arg(STI::STIApplication::theme()->group1("text").name(QColor::HexArgb))
                .arg(STI::STIApplication::theme()->group3("background").name(QColor::HexArgb))
                .arg(STI::STIApplication::theme()->group1("background").name(QColor::HexArgb))
                .arg(STI::STIApplication::theme()->group2("background").name(QColor::HexArgb))
                .arg(STI::STIApplication::theme()->group3("text").name(QColor::HexArgb))
                .arg("#dadbde")
                .arg("#f6f7fa")
                .arg(STI::STIApplication::theme()->group1("accent-1").name(QColor::HexArgb))
                .arg(background1.name(QColor::HexArgb))); 
          
  mConfigurationComboBox->setStyleSheet(QString(".QComboBox{background-color:transparent;"
                                        "border-color: gray;"
                                        "border-width: 1px;"
                                        "border-style: solid;"
                                        "padding: 1px 0px 0px 2px;"
                                        "margin: 0 5 0 5;"
                                        "}"//the padding/border/margin change is what makes the color change work - stupid bug!!!!
                                        ".QComboBox::drop-down {background-color:transparent;"
                                        "width: 15px; height: 15px;"
                                        "}"
                                        ".QComboBox::down-arrow {"
                                        "image:url(\"%1\");"
                                        "width: 10px; height: 10px;"
                                        "}").arg(mComboboxDownArrow.fileName())); 
  setMinimumWidth(700);
  
  mFileTypes = STI::Settings::FileTypes().List();
  QMap<QString, STI::Settings::FileTypes::Type>::const_iterator i = mFileTypes.constBegin();
  while (i != mFileTypes.constEnd()) {
    QString extensionString = i.key();
    extensionString.replace(".","");
    mTypeList.append(STI::Settings::FileTypes::GetText(i.value()));
    mFileExtensions.append(extensionString);
    ++i;
  }

  mTypeList.removeDuplicates();

  QList<QWizard::WizardButton> layout;
  layout << QWizard::Stretch << QWizard::BackButton << QWizard::NextButton << QWizard::FinishButton;
  setButtonLayout(layout);
    
  setPage(Page_SourceCode, new SourceCodePage(this));
  setPage(Page_ProjectFile, new ProjectFilePage(this));
  setPage(Page_MissingCMakeImport, new MissingCMakeImportPage(this));
  setPage(Page_Import, new ImportPage(this));
  setPage(Page_ImportVisualStudioDetails, new ImportVisualStudioDetailsPage(this));
  setPage(Page_ImportJsonDetails, new ImportJsonDetailsPage(this));
  setPage(Page_WatchBuild, new WatchBuildPage(this));
  setPage(Page_StrictDetails, new StrictDetailsPage(this));
  setPage(Page_Conclusion, new ConclusionPage(this));

  setStartId(Page_SourceCode);

  setWizardStyle(QWizard::ModernStyle);

  setOption(HaveHelpButton, true);
  setOption(HelpButtonOnRight, true);
  setOption(NoBackButtonOnStartPage, true);
  setOption(NoCancelButton, true);
  setOption(IndependentPages, false);
  
  //setPixmap(QWizard::BackgroundPixmap, QPixmap(":/images/understand-icon-350.png"));

  connect(this, &QWizard::helpRequested, this, &NewWizard::showHelp);
  connect(this, &QWizard::currentIdChanged, this, &NewWizard::idChanged);

  setWindowTitle(tr("New Project Wizard"));
  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

  disconnect( button( QWizard::CancelButton ), SIGNAL( clicked() ), this, SLOT( cancelWizard() ) );
  connect( button( QWizard::CancelButton ), SIGNAL( clicked() ), this, SLOT( cancelWizard() ) );
  
  initOptions();
}

void NewWizard::initOptions()
{
  int width;
  
  // Ada Version
  mAdaVersionComboBox = new ButtonComboBox();
  for (int init=0; (Settings::AdaOptions::Version)init != Settings::AdaOptions::INVALID_VERSION; init++) {
    mAdaVersionComboBox->addItem(Settings::AdaOptions::TextVersion((Settings::AdaOptions::Version) init));
  }
  mAdaVersionComboBox->setCurrentIndex((int) Settings::AdaOptions::Version::Ada95);
  mAdaVersionComboBox->setSizeAdjustPolicy(QComboBox::AdjustToContents);
  mAdaVersionComboBox->setEditable(true);
  mAdaVersionComboBox->lineEdit()->setReadOnly(true);
  mAdaVersionComboBox->view()->setTextElideMode(Qt::ElideNone); 
  mAdaVersionComboBox->lineEdit()->setAlignment(Qt::AlignRight);  
  // get the minimum width that fits the largest item.
  width = mAdaVersionComboBox->minimumSizeHint().width();
  // add 10 to make sure everything fits properly
  width += 10;
  // set the ComboBox to that width.
  mAdaVersionComboBox->setMinimumWidth(width);

  // Assembly Assembler
  mAssemblyAssemblerComboBox = new ButtonComboBox();
  foreach (QString assembler, Settings::AssemblyOptions::ListAssemblers()) {
    mAssemblyAssemblerComboBox->addItem(assembler);
  }
  mAssemblyAssemblerComboBox->setCurrentIndex((int) Settings::AssemblyOptions::LookupAssembler("Coldfire 68K"));
  mAssemblyAssemblerComboBox->setSizeAdjustPolicy(QComboBox::AdjustToContents);
  mAssemblyAssemblerComboBox->setEditable(true);
  mAssemblyAssemblerComboBox->lineEdit()->setReadOnly(true);
  mAssemblyAssemblerComboBox->view()->setTextElideMode(Qt::ElideNone); 
  mAssemblyAssemblerComboBox->lineEdit()->setAlignment(Qt::AlignRight);  
  // get the minimum width that fits the largest item.
  width = mAssemblyAssemblerComboBox->minimumSizeHint().width();
  // add 10 to make sure everything fits properly
  width += 10;
  // set the ComboBox to that width.
  mAssemblyAssemblerComboBox->setMinimumWidth(width);

  // Cobol Compiler
  mCobolCompilerComboBox = new ButtonComboBox();
  for (int init=0; (Settings::CobolOptions::Compiler)init != Settings::CobolOptions::INVALID_COMPILER; init++) {
    mCobolCompilerComboBox->addItem(Settings::CobolOptions::GetCompilerName((Settings::CobolOptions::Compiler) init));
  }
  mCobolCompilerComboBox->setCurrentIndex((int) Settings::CobolOptions::Compiler::Ansi85);
  mCobolCompilerComboBox->setSizeAdjustPolicy(QComboBox::AdjustToContents);
  mCobolCompilerComboBox->setEditable(true);
  mCobolCompilerComboBox->lineEdit()->setReadOnly(true);
  mCobolCompilerComboBox->view()->setTextElideMode(Qt::ElideNone); 
  mCobolCompilerComboBox->lineEdit()->setAlignment(Qt::AlignRight);  
  // get the minimum width that fits the largest item.
  width = mCobolCompilerComboBox->minimumSizeHint().width();
  // add 10 to make sure everything fits properly
  width += 10;
  // set the ComboBox to that width.
  mCobolCompilerComboBox->setMinimumWidth(width);

  // Cobol Format
  mCobolFormatComboBox = new ButtonComboBox();
  for (int init=0; (Settings::CobolOptions::Format)init != Settings::CobolOptions::INVALID_FORMAT; init++) {
    QString formatOption = Settings::CobolOptions::GetFormatName((Settings::CobolOptions::Format) init);
    formatOption.replace("free","Free");
    formatOption.replace("fixed","Fixed");
    formatOption.append(" Format");
    mCobolFormatComboBox->addItem(formatOption, Settings::CobolOptions::GetFormatName((Settings::CobolOptions::Format) init));
  }
  mCobolFormatComboBox->setCurrentIndex((int) Settings::CobolOptions::Format::FixedFormat);
  mCobolFormatComboBox->setSizeAdjustPolicy(QComboBox::AdjustToContents);
  mCobolFormatComboBox->setEditable(true);
  mCobolFormatComboBox->lineEdit()->setReadOnly(true);
  mCobolFormatComboBox->view()->setTextElideMode(Qt::ElideNone); 
  mCobolFormatComboBox->lineEdit()->setAlignment(Qt::AlignRight);  
  // get the minimum width that fits the largest item.
  width = mCobolFormatComboBox->minimumSizeHint().width();
  // add 10 to make sure everything fits properly
  width += 10;
  // set the ComboBox to that width.
  mCobolFormatComboBox->setMinimumWidth(width);

  // Cpp Compiler
  mCppCompilerComboBox = new ButtonComboBox();
  foreach (QString compiler, Settings::CppOptions::ListCompilers()) {
    mCppCompilerComboBox->addItem(compiler);
  }
  mCppCompilerComboBox->setCurrentIndex(mCppCompilerComboBox->findText(Settings::CppOptions::GetCompilerDefault()));
  mCppCompilerComboBox->setSizeAdjustPolicy(QComboBox::AdjustToContents);
  mCppCompilerComboBox->setEditable(true);
  mCppCompilerComboBox->lineEdit()->setReadOnly(true);
  mCppCompilerComboBox->view()->setTextElideMode(Qt::ElideNone); 
  mCppCompilerComboBox->lineEdit()->setAlignment(Qt::AlignRight);  
  // get the minimum width that fits the largest item.
  width = mCppCompilerComboBox->minimumSizeHint().width();
  // add 10 to make sure everything fits properly
  width += 10;
  // set the ComboBox to that width.
  mCppCompilerComboBox->setMinimumWidth(width);
  
  // Cpp Strict Compiler
  mCppStrictCompilerComboBox = new ButtonComboBox();
  foreach (const QString &name, kDefaultCompilers.keys())
    mCppStrictCompilerComboBox->addItem(name);
  mCppStrictCompilerComboBox->insertSeparator(mCppStrictCompilerComboBox->count());
  foreach (QString compiler, STI::Compilers::Compiler::list(true)) {
    mCppStrictCompilerComboBox->addItem(compiler);
  }
  // Map to preferred name.
  QString name = Settings::CppOptions::GetCompilerDefault();
  auto end = kDefaultCompilers.constEnd();
  for (auto it = kDefaultCompilers.constBegin(); it != end; ++it) {
    if (it.value() == name) {
      name = it.key();
      break;
    }
  }
  mCppStrictCompilerComboBox->setCurrentIndex(mCppStrictCompilerComboBox->findText(name));
  mCppStrictCompilerComboBox->setSizeAdjustPolicy(QComboBox::AdjustToContents);
  mCppStrictCompilerComboBox->setEditable(true);
  mCppStrictCompilerComboBox->lineEdit()->setReadOnly(true);
  mCppStrictCompilerComboBox->view()->setTextElideMode(Qt::ElideNone); 
  mCppStrictCompilerComboBox->lineEdit()->setAlignment(Qt::AlignRight);  
  // get the minimum width that fits the largest item.
  width = mCppStrictCompilerComboBox->minimumSizeHint().width();
  // add 10 to make sure everything fits properly
  width += 10;
  // set the ComboBox to that width.
  mCppStrictCompilerComboBox->setMinimumWidth(width);
  mCppStrictCompilerComboBox->view()->setItemDelegate(new ComboBoxDelegate);

  // Fortran Version
  mFortranVersionComboBox = new ButtonComboBox();
  for (int init=0; (Settings::FortranOptions::Version)init != Settings::FortranOptions::INVALID_VERSION; init++) {
    mFortranVersionComboBox->addItem(Settings::FortranOptions::TextVersion((Settings::FortranOptions::Version) init));
  }
  mFortranVersionComboBox->setCurrentIndex((int) Settings::FortranOptions::Version::Fortran90);
  mFortranVersionComboBox->setSizeAdjustPolicy(QComboBox::AdjustToContents);
  mFortranVersionComboBox->setEditable(true);
  mFortranVersionComboBox->lineEdit()->setReadOnly(true);
  mFortranVersionComboBox->view()->setTextElideMode(Qt::ElideNone); 
  mFortranVersionComboBox->lineEdit()->setAlignment(Qt::AlignRight);  
  // get the minimum width that fits the largest item.
  width = mFortranVersionComboBox->minimumSizeHint().width();
  // add 10 to make sure everything fits properly
  width += 10;
  // set the ComboBox to that width.
  mFortranVersionComboBox->setMinimumWidth(width);

  // Fortran Format
  mFortranFormatComboBox = new ButtonComboBox();
  for (int init=0; (Settings::FortranOptions::Format)init != Settings::FortranOptions::INVALID_FORMAT; init++) {
    QString formatOption = Settings::FortranOptions::TextFormat((Settings::FortranOptions::Format) init);
    formatOption.append(" Format");
    mFortranFormatComboBox->addItem(formatOption, Settings::FortranOptions::TextFormat((Settings::FortranOptions::Format) init));
  }
  mFortranFormatComboBox->setCurrentIndex((int) Settings::FortranOptions::Format::Auto);
  mFortranFormatComboBox->setSizeAdjustPolicy(QComboBox::AdjustToContents);
  mFortranFormatComboBox->setEditable(true);
  mFortranFormatComboBox->lineEdit()->setReadOnly(true);
  mFortranFormatComboBox->view()->setTextElideMode(Qt::ElideNone); 
  mFortranFormatComboBox->lineEdit()->setAlignment(Qt::AlignRight);  
  // get the minimum width that fits the largest item.
  width = mFortranFormatComboBox->minimumSizeHint().width();
  // add 10 to make sure everything fits properly
  width += 10;
  // set the ComboBox to that width.
  mFortranFormatComboBox->setMinimumWidth(width);

  // Java Version
  mJavaVersionComboBox = new ButtonComboBox();
  for (int init=0; (Settings::JavaOptions::Version)init != Settings::JavaOptions::INVALID_VERSION; init++) {
    mJavaVersionComboBox->addItem(Settings::JavaOptions::TextVersion((Settings::JavaOptions::Version) init));
  }
  mJavaVersionComboBox->setCurrentIndex((int) Settings::JavaOptions::Version::Java5);
  mJavaVersionComboBox->setSizeAdjustPolicy(QComboBox::AdjustToContents);
  mJavaVersionComboBox->setEditable(true);
  mJavaVersionComboBox->lineEdit()->setReadOnly(true);
  mJavaVersionComboBox->view()->setTextElideMode(Qt::ElideNone); 
  mJavaVersionComboBox->lineEdit()->setAlignment(Qt::AlignRight);  
  // get the minimum width that fits the largest item.
  width = mJavaVersionComboBox->minimumSizeHint().width();
  // add 10 to make sure everything fits properly
  width += 10;
  // set the ComboBox to that width.
  mJavaVersionComboBox->setMinimumWidth(width);

  // Jovial Version
  mJovialVersionComboBox = new ButtonComboBox();
  for (int init=0; (Settings::JovialOptions::Version)init != Settings::JovialOptions::INVALID_VERSION; init++) {
    mJovialVersionComboBox->addItem(Settings::JovialOptions::TextVersion((Settings::JovialOptions::Version) init));
  }
  mJovialVersionComboBox->setCurrentIndex((int) Settings::JovialOptions::Version::Jovial73);
  mJovialVersionComboBox->setSizeAdjustPolicy(QComboBox::AdjustToContents);
  mJovialVersionComboBox->setEditable(true);
  mJovialVersionComboBox->lineEdit()->setReadOnly(true);
  mJovialVersionComboBox->view()->setTextElideMode(Qt::ElideNone); 
  mJovialVersionComboBox->lineEdit()->setAlignment(Qt::AlignRight);  
  // get the minimum width that fits the largest item.
  width = mJovialVersionComboBox->minimumSizeHint().width();
  // add 10 to make sure everything fits properly
  width += 10;
  // set the ComboBox to that width.
  mJovialVersionComboBox->setMinimumWidth(width);

  // Pascal Version
  mPascalVersionComboBox = new ButtonComboBox();
  for (int init=0; (Settings::PascalOptions::Version)init != Settings::PascalOptions::INVALID_VERSION; init++) {
    mPascalVersionComboBox->addItem(Settings::PascalOptions::TextVersion((Settings::PascalOptions::Version) init));
  }
  mPascalVersionComboBox->setCurrentIndex((int) Settings::PascalOptions::Version::Delphi);
  mPascalVersionComboBox->setSizeAdjustPolicy(QComboBox::AdjustToContents);
  mPascalVersionComboBox->setEditable(true);
  mPascalVersionComboBox->lineEdit()->setReadOnly(true);
  mPascalVersionComboBox->view()->setTextElideMode(Qt::ElideNone); 
  mPascalVersionComboBox->lineEdit()->setAlignment(Qt::AlignRight);  
  // get the minimum width that fits the largest item.
  width = mPascalVersionComboBox->minimumSizeHint().width();
  // add 10 to make sure everything fits properly
  width += 10;
  // set the ComboBox to that width.
  mPascalVersionComboBox->setMinimumWidth(width);

  // Plm Version
  mPlmVersionComboBox = new ButtonComboBox();
  for (int init=0; (Settings::PlmOptions::Version)init != Settings::PlmOptions::INVALID_VERSION; init++) {
    mPlmVersionComboBox->addItem(Settings::PlmOptions::TextVersion((Settings::PlmOptions::Version) init));
  }
  mPlmVersionComboBox->setCurrentIndex((int) Settings::PlmOptions::Version::Plm80);
  mPlmVersionComboBox->setSizeAdjustPolicy(QComboBox::AdjustToContents);
  mPlmVersionComboBox->setEditable(true);
  mPlmVersionComboBox->lineEdit()->setReadOnly(true);
  mPlmVersionComboBox->view()->setTextElideMode(Qt::ElideNone); 
  mPlmVersionComboBox->lineEdit()->setAlignment(Qt::AlignRight);  
  // get the minimum width that fits the largest item.
  width = mPlmVersionComboBox->minimumSizeHint().width();
  // add 10 to make sure everything fits properly
  width += 10;
  // set the ComboBox to that width.
  mPlmVersionComboBox->setMinimumWidth(width);

  // Python Version
  mPythonVersionComboBox = new ButtonComboBox();
  for (int init=0; (Settings::PythonOptions::Version)init != Settings::PythonOptions::INVALID_VERSION; init++) {
    mPythonVersionComboBox->addItem(Settings::PythonOptions::TextVersion((Settings::PythonOptions::Version) init));
  }
  mPythonVersionComboBox->setCurrentIndex((int) Settings::PythonOptions::Version::Python3);
  mPythonVersionComboBox->setSizeAdjustPolicy(QComboBox::AdjustToContents);
  mPythonVersionComboBox->setEditable(true);
  mPythonVersionComboBox->lineEdit()->setReadOnly(true);
  mPythonVersionComboBox->view()->setTextElideMode(Qt::ElideNone); 
  mPythonVersionComboBox->lineEdit()->setAlignment(Qt::AlignRight);  
  // get the minimum width that fits the largest item.
  width = mPythonVersionComboBox->minimumSizeHint().width();
  // add 10 to make sure everything fits properly
  width += 10;
  // set the ComboBox to that width.
  mPythonVersionComboBox->setMinimumWidth(width);

  // Web Version
  mWebVersionComboBox = new ButtonComboBox();
  for (int init=0; (Settings::WebOptions::PhpVersion)init != Settings::WebOptions::INVALID_PHP_VERSION; init++) {
    QString webVersion = Settings::WebOptions::TextPhpVersion((Settings::WebOptions::PhpVersion) init);
    webVersion.prepend("PHP ");
    mWebVersionComboBox->addItem(webVersion, Settings::WebOptions::TextPhpVersion((Settings::WebOptions::PhpVersion) init));
  }
  
  QString defaultWebVersion = Settings::WebOptions::TextPhpVersion(Settings::WebOptions::PhpVersion::Php53);
  defaultWebVersion.prepend("PHP ");
  mWebVersionComboBox->setCurrentIndex(mWebVersionComboBox->findText(defaultWebVersion));
  mWebVersionComboBox->setSizeAdjustPolicy(QComboBox::AdjustToContents);
  mWebVersionComboBox->setEditable(true);
  mWebVersionComboBox->lineEdit()->setReadOnly(true);
  mWebVersionComboBox->view()->setTextElideMode(Qt::ElideNone); 
  mWebVersionComboBox->lineEdit()->setAlignment(Qt::AlignRight);  
  // get the minimum width that fits the largest item.
  width = mWebVersionComboBox->minimumSizeHint().width();
  // add 10 to make sure everything fits properly
  width += 10;
  // set the ComboBox to that width.
  mWebVersionComboBox->setMinimumWidth(width);

  mInitializeOptions = false;
}

void NewWizard::cancelWizard()
{
  mResultStatus = NewWizard_CANCEL;
  reject();
}

void NewWizard::clearAllSettings()
{
  mDirectoryList.clear();
  mSearchContents.clear();
  mExistingUdbList.clear();
  mExistingJsonList.clear();
  mExistingVisualStudioList.clear();
  mHasCPPFiles = false;
  mHasImportCPPFiles = false;
  mHasCMakeFiles = false;
}

void NewWizard::openProjectWizard()
{
  mResultStatus = NewWizard_OPEN;
  
  reject();
}

void NewWizard::idChanged(int id)
{  
  button(QWizard::NextButton)->setText(tr("Skip"));
  button(QWizard::BackButton)->setText(tr("Back"));
  
  if (id == Page_SourceCode) {
    button(QWizard::NextButton)->setText(tr("Continue"));
  }

  if (id == Page_ProjectFile) {
    //QSettings settings;
    //bool useClang = settings.value("use_clang",false).toBool();
    //mStrictRadioButton->setChecked(useClang);
    //mFuzzyRadioButton->setChecked(!useClang);
    button(QWizard::NextButton)->setText(tr("Skip"));
  }

  if (id == Page_ImportVisualStudioDetails) {
    mStrictRadioButton->setChecked(true);
    mFuzzyRadioButton->setChecked(false);
    button(QWizard::NextButton)->setText(tr("Continue"));
  }
  if (id == Page_ImportJsonDetails) {
    mStrictRadioButton->setChecked(true);
    mFuzzyRadioButton->setChecked(false);
    button(QWizard::NextButton)->setText(tr("Continue"));
  }
  if (id == Page_WatchBuild) {
    //mStrictRadioButton->setChecked(true);
    //mFuzzyRadioButton->setChecked(false);
    button(QWizard::NextButton)->setText(tr("Skip"));
  }
  if (id == Page_StrictDetails) {
    button(QWizard::NextButton)->setText(tr("Continue"));
  }
  if (id == Page_Import) {
    QSettings settings;
    bool useClang = settings.value("use_clang",true).toBool();
    mStrictRadioButton->setChecked(useClang);
    mFuzzyRadioButton->setChecked(!useClang);
  }
  if (id == Page_Conclusion) {
    if (mInitializeOptions) {
      initOptions(); // reinitialize in case the user has been to the conclusion page and back
    }
     button(QWizard::FinishButton)->setText(tr("Create Project"));
  }
}

void NewWizard::showHelp()
{
  QString message;

  switch (currentId()) {
    case Page_SourceCode:
      //message = tr("<font size=\"+2\"><Strong>Where Is Your Source Code?</Strong></font><br><br>"
      message = tr("Specify the root directory or directories where the source code for a single executable resides. If your source has multiple targets you may need a separate Understand project for each.");
      break;
    case Page_ProjectFile:
      //message = tr("<font size=\"+2\"><Strong>Open Existing Understand Project?</Strong></font><br><br>"
      message = tr("We found the following Understand project(s) while analyzing the specified directories. "
                   "Lucky you, it looks like someone else may have already set everything up. "
                   "The project may contain everything you want to analyze, open it to explore your code.");
      break;
    case Page_MissingCMakeImport:
      //message = tr("<font size=\"+2\"><Strong>Import Settings from CMake?</Strong></font><br><br>"
      message = tr("We found CMake build files in your code. CMake can create a compilation database named compile_commands.json "
                   "that has everything we need for an accurate Understand project.  Would you like to specify where this file is located? ");
      break;
    case Page_Import:
      //message = tr("<font size=\"+2\"><Strong>Import Settings from Other Tools?</Strong></font><br><br>"
      message = tr("To accurately analyze your code, we need information like Include Paths and Macro definitions. "
                   "We can import these from other sources like Visual Studio Project and Solution files or CMake Compile Commands. "
                   "If we do create the Understand project from one of these imported files it may not include everything from the directories specified earlier.");
      break;
    case Page_ImportVisualStudioDetails:
      //message = tr("<font size=\"+2\"><Strong>Specify Visual Studio Configuration?</Strong></font><br><br>"
      message = tr("The configuration state determines what files to include in the project and what macro values are set.");
      break;
    case Page_ImportJsonDetails:
      //message = tr("<font size=\"+2\"><Strong>CMake File</Strong></font><br><br>"
      message = tr("Normally the Understand project stays synchronized with the CMake project, "
                   "so if a file is added to the CMake project it will automatically be added to the Understand project. "
                   "Check this option if you do not want them synchronized. "
                   "We will still create your initial Understand project with the same settings as the CMake project.");
      break;
    case Page_WatchBuild:
      message = tr("Watch your build.");
      break;
    case Page_StrictDetails:
      //message = tr("<font size=\"+2\"><Strong>Strict or Fuzzy</Strong></font><br><br>"
      message = tr("C/C++ Fuzzy Analysis: Great for the first pass at most "
                   "code, since very little setup is required. Uses fuzzy logic to handle "
                   "incomplete, non-compiling code gracefully and as accurately as possible.<br><br>"
                   "C/C++ Strict Analysis: This option may result in a more accurate analysis, "
                   "so more setup is required. Include paths and macros will need to be "
                   "defined during the analysis. Handles C++ templates and overloaded functions "
                   "better than the fuzzy analyzer. It also will analyze Objective C/C++, C++11 "
                   "and C++14. Include paths and macros need to be defined correctly with this "
                   "option or Understand will return invalid or incomplete results.\n");
      break;
    case Page_Conclusion:
      message = tr("Create .udb help.");
      break;
    default:
      message = tr("This help is likely not to be of any help.");
  }

  //QMessageBox msgBox;
  ////msgBox.setWindowTitle(tr("New Project Wizard Help"));
  //msgBox.setText(message);
  //msgBox.setIcon(QMessageBox::Information);
  //msgBox.setStandardButtons(QMessageBox::Ok);
  //msgBox.setDefaultButton(QMessageBox::Ok);
  ////msgBox.setWindowModality(Qt::ApplicationModal);
  //msgBox.setWindowFlags(Qt::FramelessWindowHint|Qt::WindowStaysOnTopHint);
  //int ret = msgBox.exec();
  ////QMessageBox::information(this, tr("New Project Wizard Help"), message);
  
  QMessageBox mb(QMessageBox::Information,
                     tr("New Project Wizard Help"), 
                     message, 
                     QMessageBox::Ok, this,
                     Qt::FramelessWindowHint|Qt::WindowStaysOnTopHint);
  mb.exec();
}

void NewWizard::changeProgressDialog() {
  if (mNumberOfToBeAddedFiles > 0) {
    double toBeAddedDouble = mNumberOfToBeAddedFiles;
    mFilesToBeAddedString = tr("out of: %1").arg(QLocale::system().toString(toBeAddedDouble, 'f', 0));
    mProgressDialog->setRange(0, mNumberOfToBeAddedFiles);
  }
}

void NewWizard::updateProgressDialog()
{
  double tempAddedDirectories = mNumberOfAddedDirectories;
  double tempAddedFiles = mNumberOfAddedFiles;
  if (mPathTaken == Path_Standard) {
    mProgressDialog->setLabelText(tr("Directories Searched: %1\nProject Files Added: %2 %3").arg(QLocale::system().toString(tempAddedDirectories, 'f', 0)).arg(QLocale::system().toString(tempAddedFiles, 'f', 0)).arg(mFilesToBeAddedString));
  } else {
    mProgressDialog->setLabelText(tr("Project Files Added: %2 %3").arg(QLocale::system().toString(tempAddedFiles, 'f', 0)).arg(mFilesToBeAddedString));
  }
    mProgressDialog->setValue(mNumberOfAddedFiles);
}

void NewWizard::accept()
{
  STI::ManagedDb::DatabaseRef db;
  QString filename = mProjectNameLineEdit->text();
  QFile file(filename);
  if (file.exists() ) {
    
    // Prompt to close db.
    QMessageBox::StandardButton button =
      PersistentMessageBox::information(
        this,
        tr("Project File Exists"),
        tr("The project file %1 already exists.\nOverwrite this file?").arg(filename),
        QMessageBox::Ok | QMessageBox::Cancel);

    if (button != QMessageBox::Ok)
      return;

    if (!file.remove()) {
      QMessageBox::information(this, tr("New Project Wizard Help"), tr("Could not delete existing file. \nFile: %1").arg(filename));
      return;
    }
  } 
  
  int overallFileCount = 0;
  //
  // Init the progress dialog
  mProgressDialog = new QProgressDialog(tr("Adding Files..."), tr("Cancel"), 0, 100, this);
  mProgressDialog->setModal(true);
  mProgressDialog->setWindowFlags(mProgressDialog->windowFlags() | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
  mProgressDialog->setMinimumDuration(300);
  mProgressDialog->setLabelText(tr("Adding files: %1").arg("0"));
  mProgressDialog->setRange(0, 0);

  
  //need tp fix slashes to OS
#ifdef WIN32
  filename.replace("/","\\");
#else
  filename.replace("\\","/");
#endif
  mProjectName = filename;
  //if (STI::Configure::CreateProject(filename)) {
  if (Udb::Db::create(filename)) {
    STI::ManagedDb::Status status;
    
    db=STI::ManagedDb::DatabaseManager::Manager().GetOpenDatabase(filename,status,false,false);
    
    QCoreApplication::instance()->processEvents();

    if (!status.Okay()) {
      QString qs("Could not get database instance for project: ");
      qs.append(filename);
      qs.append("\nDesc: ");
      qs.append(status.Description());
      QMessageBox mb("Warning", qs, QMessageBox::Warning, QMessageBox::Ok,
                     QMessageBox::NoButton, QMessageBox::NoButton, QApplication::activeWindow());
      mb.exec();
      return;
    }
    
    if (db && db->IsOpen()) {
      mResultStatus = NewWizard_CANCEL;
      
      Settings::ValuesRef proj = Settings::Values::Current(db);
      
      Settings::Languages & languageOptions = proj->Languages();
      
      // ensure that all languages are off - no defaults
      languageOptions.Set(Settings::Languages::Ada, false);
      languageOptions.Set(Settings::Languages::Assembly, false);
      languageOptions.Set(Settings::Languages::Basic, false);
      languageOptions.Set(Settings::Languages::Cobol, false);
      languageOptions.Set(Settings::Languages::Cpp, false);
      languageOptions.Set(Settings::Languages::Csharp, false);
      languageOptions.Set(Settings::Languages::Fortran, false);
      languageOptions.Set(Settings::Languages::Java, false);
      languageOptions.Set(Settings::Languages::Jovial, false);
      languageOptions.Set(Settings::Languages::Pascal, false);
      languageOptions.Set(Settings::Languages::Plm, false);
      languageOptions.Set(Settings::Languages::Python, false);
      languageOptions.Set(Settings::Languages::Vhdl, false);
      languageOptions.Set(Settings::Languages::Web, false);

      
      QMap<QString, int> tempSearchContents;
      if (mPathTaken == NewWizard::Path_Standard) {
        tempSearchContents = mSearchContents;
      } else {
        tempSearchContents = mImportSearchContents;
      }
      
      std::set<Settings::FileTypes::Type> filters;
      foreach (QString languageKey, tempSearchContents.keys()) {
        if (!languageKey.startsWith(".")) {
          languageKey.prepend(".");
        }

        STI::Settings::FileTypes::Type type = proj->FileTypes().GetType(languageKey);
        Settings::Languages::Language lang = Settings::FileTypes::Language(type);
        
        if (lang != Settings::Languages::INVALID_LANGUAGE) {
          languageOptions.Set(lang, true);
          filters.insert(type);
        }
      }
      


      // set the language options
      for (int i = 0; i < mLanguageOptionsListWidget->count(); i++) {
        QListWidgetItem * optionItem = mLanguageOptionsListWidget->item(i);
        Settings::Languages::Language languageKey = Settings::Languages::INVALID_LANGUAGE;

        if (optionItem && optionItem->data(Qt::UserRole).canConvert(QVariant::String)) {
          //QString itemString = (QString)optionItem->data(Qt::UserRole).toString();
          QString itemString = "Other";
          languageKey = static_cast<Settings::Languages::Language>(optionItem->data(Qt::UserRole).toInt());
          if (Settings::Languages::Valid(languageKey)) {
            itemString = Settings::Languages::Text(languageKey);
          }
          itemString = itemString;
        }
        switch (languageKey) {
          case Settings::Languages::Ada: {
            if (proj->AdaOptions().GetStandardLibraryDirectory().isEmpty()){
              proj->AdaOptions().SetStandardLibraryDirectory(proj->AdaOptions().DefaultStandardLibraryDirectory());
            }
            int adaPosition = mAdaVersionComboBox->currentIndex();
            QString adaText = mAdaVersionComboBox->currentText();
            if (proj->AdaOptions().ValidVersion(proj->AdaOptions().LookupVersion(adaText))) {
              proj->AdaOptions().SetVersion(proj->AdaOptions().LookupVersion(adaText));
            }
            break;
          }
          case Settings::Languages::Assembly: {
            int assemblyPosition = mAssemblyAssemblerComboBox->currentIndex();
            QString assemblyText = mAssemblyAssemblerComboBox->currentText();
            proj->AssemblyOptions().SetAssembler(assemblyText);
            break;
          }
          case Settings::Languages::Basic: // has no additional configuration options at this time
            break;
          case Settings::Languages::Cobol: {
            int cobolPosition = mCobolCompilerComboBox->currentIndex();
            QString cobolText = mCobolCompilerComboBox->currentText();
            proj->CobolOptions().SetCompiler((Settings::CobolOptions::Compiler) cobolPosition);
            
            int cobolFormatPosition = mCobolFormatComboBox->currentIndex();
            QString cobolFormatText = mCobolFormatComboBox->currentText();
            proj->CobolOptions().SetFormat((Settings::CobolOptions::Format) cobolFormatPosition);
            break;
          }
          case Settings::Languages::Cpp: {
            Settings::CppOptions & cppOptions = proj->CppOptions();
            cppOptions.SetUseClang(mStrictRadioButton->isChecked());
            if (mStrictRadioButton->isChecked()) {
              int cppStrictPosition = mCppStrictCompilerComboBox->currentIndex();
              QString cppStrictText = mCppStrictCompilerComboBox->currentText();
              cppOptions.SetCompiler(kDefaultCompilers.value(cppStrictText, cppStrictText));
            } else {
              int cppPosition = mCppCompilerComboBox->currentIndex();
              QString cppText = mCppCompilerComboBox->currentText();
              cppOptions.SetCompiler(cppText);
            }
            break;
          }
          case Settings::Languages::Csharp: // has no additional configuration options at this time
            break;
          case Settings::Languages::Fortran: {
            int fortranVersionPosition = mFortranVersionComboBox->currentIndex();
            QString fortranVersionText = mFortranVersionComboBox->currentText();
            proj->FortranOptions().SetVersion((Settings::FortranOptions::Version) fortranVersionPosition);
            int fortranFormatPosition = mFortranFormatComboBox->currentIndex();
            QString fortranFormatText = mFortranFormatComboBox->currentText();
            proj->FortranOptions().SetFormat((Settings::FortranOptions::Format) fortranFormatPosition);
            break; 
          }
          case Settings::Languages::Java: {
            int javaPosition = mJavaVersionComboBox->currentIndex();
            QString javaText = mJavaVersionComboBox->currentText();
            proj->JavaOptions().SetVersion((Settings::JavaOptions::Version) javaPosition);
            break;
          }
          case Settings::Languages::Jovial: {
            int jovialPosition = mJovialVersionComboBox->currentIndex();
            QString jovialText = mJovialVersionComboBox->currentText();
            proj->JovialOptions().SetVersion((Settings::JovialOptions::Version) jovialPosition);
            break;
          }
          case Settings::Languages::Pascal: {
            int pascalPosition = mPascalVersionComboBox->currentIndex();
            QString pascalText = mPascalVersionComboBox->currentText();
            proj->PascalOptions().SetVersion((Settings::PascalOptions::Version) pascalPosition);
            break;
          }
          case Settings::Languages::Plm: {
            int plmPosition = mPlmVersionComboBox->currentIndex();
            QString plmText = mPlmVersionComboBox->currentText();
            proj->PlmOptions().SetVersion((Settings::PlmOptions::Version) plmPosition);
            break;
          }
          case Settings::Languages::Python: {
            proj->PythonOptions().SetUseInstalledStandard(false);
            int pythonPosition = mPythonVersionComboBox->currentIndex();
            QString pythonText = mPythonVersionComboBox->currentText();
            proj->PythonOptions().SetVersion((Settings::PythonOptions::Version) pythonPosition);
            break;
          }
          case Settings::Languages::Vhdl:
            break;
          case Settings::Languages::Web: {
            int webPosition = mWebVersionComboBox->currentIndex();
            QString webText = mWebVersionComboBox->currentText();
            proj->WebOptions().SetPhpVersion((Settings::WebOptions::PhpVersion) webPosition);
            break;
          }
          default:
            break;
        }

      }

      mBreakout = false;
      Settings::Files &projectFiles = proj->Files();
      projectFiles.SetDefaultAddMode(Settings::Files::Absolute);

      if (mPathTaken == Path_ImportCMake) {
        mProgressDialog->setLabelText(tr("Adding files"));
        // show process indicator
        mProgressDialog->show();
        mNumberOfAddedFiles = 0;
        mNumberOfToBeAddedFiles = 0;
        mNumberOfAddedDirectories = 0;
        connect(mProgressDialog, &QProgressDialog::canceled, [this]
        {
          mBreakout = true;
        });

        QCoreApplication::instance()->processEvents();

        Settings::CMakeOptions &opts = proj->CMakeOptions();
        opts.setFiles(opts.files() << mImportName);
        
        //// Synchronize all CMake compile command databases.
        //Settings::CMakeOptions &cmakeOpts = proj->CMakeOptions();
        //CMake::CompileDatabase cd(mImportName);
        //Settings::Files &root = proj->Files();
        //if (cd.isValid())
        //  cd.synchronize(db,root,cmakeOpts.excludeFilter(mImportName));
        
        NewWizardMessage addMessage(this);

        Settings::SyncFile::RescanSyncProjects(db, proj, mCMakeOneTimeImport->isChecked(), &addMessage);
      } else if (mPathTaken == Path_ImportVisualStudio) {
        mProgressDialog->setLabelText(tr("Adding files"));
        // show process indicator
        mProgressDialog->show();
        mNumberOfAddedFiles = 0;
        mNumberOfToBeAddedFiles = 0;
        mNumberOfAddedDirectories = 0;
        connect(mProgressDialog, &QProgressDialog::canceled, [this]
        {
          mBreakout = true;
        });

        QCoreApplication::instance()->processEvents();

        Settings::VisualStudioOptions &vsopts = proj->VisualStudioOptions();
        Settings::VisualStudioOptions::FileList setlist;
        Settings::VisualStudioOptions::ExcludeList excludelist;

        QString filetxt = mImportName;
        QString conftxt = mConfigurationComboBox->currentText();
        setlist.insert(filetxt,conftxt); //should have already been through isValid checks
        QString ignoretxt = QString();
        excludelist[filetxt] = ignoretxt;

        vsopts.SetFiles(setlist);
        foreach (QString key, excludelist.keys())
          vsopts.SetExcludeFilter(key, excludelist[key]);
        
        NewWizardMessage addMessage(this);

        Settings::SyncFile::RescanSyncProjects(db, proj, mVisualStudioOneTimeImport->isChecked(), &addMessage);

      } else {
        
        QString exclude = "";
        bool subdir = true;
        QString additional = "";

        connect(mProgressDialog, &QProgressDialog::canceled, [this]
        {
          mBreakout = true;
        });

        // show process indicator
        mProgressDialog->show();
        mNumberOfAddedFiles = 0;
        mNumberOfToBeAddedFiles = 0;
        mNumberOfAddedDirectories = 0;
        
        //Adding directory
        Settings::Directory *existingDirectory;
        NewWizardMessage addMessage(this);
        
        QStringList dirNamesToUse;
        QStringList tempDirNames;
        for (int i = 0; i < mSourceCodeListWidget->count(); i++) {
          QString dirName = mSourceCodeListWidget->item(i)->text();
          if (!tempDirNames.contains(dirName)) {
            tempDirNames.append(dirName);
          }
        }

        tempDirNames.removeDuplicates();
        dirNamesToUse = tempDirNames;
        // remove all specified filenames and add after search
        foreach(QString directory, dirNamesToUse) {
          directory = QDir::toNativeSeparators(QFileInfo(directory).absoluteFilePath());

          if (projectFiles.Lookup(directory)) { //It already exists
            existingDirectory = projectFiles.Lookup(directory)->IsDir();
          } else {
            existingDirectory = projectFiles.AddDir(directory); //Create it
          }
          if (existingDirectory) { // Now it should exist
            existingDirectory->SetWatched(Settings::Directory::Watched);
            existingDirectory->SetWatchedSettings(filters,additional,exclude,subdir);
          }
        }
        projectFiles.Rescan(db,0,&addMessage);
      }

      disconnect(mProgressDialog);

      mProgressDialog->hide();

      if (mBreakout) {
        ManagedDb::DatabaseManager::Manager().CloseDatabase(db);
        // Close the progress dialog
        mProgressDialog->close();
        return;
      }
      
      // Close the progress dialog
      mProgressDialog->close();
      
      proj->Save(db);
      ManagedDb::DatabaseManager::Manager().CloseDatabase(db);
      if (mOpenAdvancedSettingsCheckbox->isChecked()) {
        mResultStatus = NewWizard_CONFIGUREREQUESTED;
      } else {
        mResultStatus = NewWizard_OPENPARSE;
      }
    }
  }
  
  QDialog::accept();
}

SourceCodePage::SourceCodePage(NewWizard * wizardParent)
  : QWizardPage(wizardParent), mParentWizard(wizardParent)
{
  setTitle(tr("Where Is Your Source Code?"));
  //setPixmap(QWizard::BackgroundPixmap, QPixmap(":/images/understandWizard.png"));
  
  topLabel = new QLabel(tr("Where is the source code for this project located? <a href='www.scitools.com'>More info</a>"));
  topLabel->setWordWrap(true);

  topLabel->setToolTip(tr("Specify the root directory or directories where the source code for a single executable resides. If your source has multiple targets you may need a separate Understand project for each."));
  connect(topLabel, &QLabel::linkActivated, [this]
  {
    mParentWizard->showHelp();
  });

  sourceCodeAddDirectoryButton = new QToolButton(this);
  sourceCodeAddDirectoryButton->setText(tr("Add Directory"));

  connect(sourceCodeAddDirectoryButton, &QToolButton::clicked, [this]() {
    
    QFileDialog * dialog = new QFileDialog(this,tr("Choose a Source Code Location"));
    
    dialog->setFileMode(QFileDialog::Directory);
    dialog->setOptions(QFileDialog::DontResolveSymlinks);

    if (dialog->exec()) {
      QStringList dirNames = dialog->selectedFiles();
      if (dirNames.size() > 0)  {
        foreach(QString dirName, dirNames) {
          if (mParentWizard->mSourceCodeListWidget->findItems(dirName, Qt::MatchFixedString).count() == 0) {

            QListWidgetItem * sourceItem = new QListWidgetItem(Icon(":/icons/folder.svg"), dirName, mParentWizard->mSourceCodeListWidget);
            mParentWizard->mSourceCodeListWidget->addItem(sourceItem);
            mParentWizard->mSourceCodeListWidget->setItemWidget(sourceItem, new QWidget());
            mParentWizard->mSourceCodeListWidget->setItemSelected(sourceItem, true);
          }
        }
      }
    }   
    mParentWizard->clearAllSettings();
  });
  
  
  connect(mParentWizard->mSourceCodeListWidget, &QListWidget::itemSelectionChanged, [this]() {
    QList<QListWidgetItem*> selected = mParentWizard->mSourceCodeListWidget->selectedItems();

    bool showSelected = selected.count() == 1;
    foreach(QListWidgetItem* item, mParentWizard->mSourceCodeListWidget->findItems("*",Qt::MatchWildcard)) {
      if (selected.contains(item)) {
        QToolButton * newButton = new QToolButton(this);
        newButton->setAutoRaise(true);
        newButton->setToolTip(tr("Remove this directory"));
        newButton->setIcon(Icon(":/icons/status-error.svg"));
        newButton->setIconSize(QSize(10,10));
        newButton->setStyleSheet(QString("QToolButton {"
                                         //"margin: 5px;"
                                         //"border-color: transparent;"
                                         "border-style: none;"
                                         //"border-radius: 3px;"
                                         //"border-width: 5px;"
                                         //"color: %1;"
                                         "background-color: transparent);"
                                         "}").arg(STI::STIApplication::theme()->group1("text").name(QColor::HexArgb))
                                         .arg(STI::STIApplication::theme()->group3("background").name(QColor::HexArgb))
                                         .arg(STI::STIApplication::theme()->group2("background").name(QColor::HexArgb))); 

        connect(newButton, &QToolButton::clicked, [this, item] {
            mParentWizard->clearAllSettings();
            int toBeDeleted = mParentWizard->mSourceCodeListWidget->row(item);
            delete mParentWizard->mSourceCodeListWidget->takeItem(toBeDeleted);
            if (toBeDeleted >= mParentWizard->mSourceCodeListWidget->count()) {
              --toBeDeleted;
            }
            mParentWizard->mSourceCodeListWidget->setCurrentRow(toBeDeleted);
          });
        
        QWidget* newWidget = new QWidget();

        QHBoxLayout * hBoxLayout = new QHBoxLayout(newWidget);
        hBoxLayout->addItem(new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum));
        hBoxLayout->addWidget(newButton, 0);
        
        hBoxLayout->setSpacing(0);
        hBoxLayout->setMargin(0);

        newWidget->setLayout(hBoxLayout);
        newWidget->setContentsMargins(0,0,0,0);

       item->setSizeHint(newWidget->sizeHint());
       mParentWizard->mSourceCodeListWidget->setItemWidget(item, newWidget);

      } else {
        mParentWizard->mSourceCodeListWidget->setItemWidget(item, new QWidget());
      }
      //QWidget * itemWidget = mParentWizard->mSourceCodeListWidget->itemWidget(item);
      //if (item && itemWidget) {
      //  if (showSelected) {
      //    itemWidget->setVisible(selected.contains(item));
      //  } else {
      //    itemWidget->setVisible(false);
      //  }
      //}
    }
  });

  QHBoxLayout * buttonLayout = new QHBoxLayout;
  buttonLayout->addWidget(sourceCodeAddDirectoryButton);
  buttonLayout->addItem(new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum));
  
  QVBoxLayout *layout = new QVBoxLayout;
  layout->addWidget(topLabel);
  layout->addLayout(buttonLayout);
  layout->addWidget(mParentWizard->mSourceCodeListWidget);
  setLayout(layout);
}


void SourceCodePage::initializePage() 
{
  mParentWizard->mPathTaken = NewWizard::Path_Standard;
  mParentWizard->button(QWizard::NextButton)->setText(tr("Continue"));
}

bool SourceCodePage::validatePage() 
{
  mParentWizard->clearAllSettings();
  bool breakout = false;
  int numberOfProcessedFiles = 0;
  if (mParentWizard->mDirectoryList.isEmpty()) {  
    //
    // Init the progress dialog
    mProgressDialog = new QProgressDialog(tr("Finding Files..."), tr("Cancel"), 0, 100, this);
    mProgressDialog->setModal(true);
    mProgressDialog->setWindowFlags(mProgressDialog->windowFlags() | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    mProgressDialog->setMinimumDuration(1000);
    mProgressDialog->setLabelText(tr("Processed files: %1").arg("0"));
    mProgressDialog->show();
    mProgressDialog->setRange(0, 0);

    QStringList dirNamesToUse;
    QStringList tempDirNames;
    for (int i = 0; i < mParentWizard->mSourceCodeListWidget->count(); i++) {
      QString dirName = mParentWizard->mSourceCodeListWidget->item(i)->text();
      if (!tempDirNames.contains(dirName)) {
        tempDirNames.append(dirName);
      }
    }

    tempDirNames.removeDuplicates();
    dirNamesToUse = tempDirNames;
    QStringList filenamesToUse;
    // remove all specified filenames and add after search
    foreach(QString tempFilename, dirNamesToUse) {
      QFileInfo filenameInfo(tempFilename);

      if (filenameInfo.isFile() && !filenamesToUse.contains(tempFilename)) {
        QString filenameExtention = filenameInfo.suffix();
        int extentionValue = 0;
        
        // Understand extension
        if (filenameExtention == "udb" || filenameExtention == "udx") {
          mParentWizard->mExistingUdbList.append(tempFilename);
        }
        
        // Visual Studio extensions
        if (filenameExtention == "csproj" || 
            filenameExtention == "dsp" || 
            filenameExtention == "dsw" || 
            filenameExtention == "sln" || 
            filenameExtention == "vcp" || 
            filenameExtention == "vcproj" ||
            filenameExtention == "vbproj" ||
            filenameExtention == "vcxproj" ||
            filenameExtention == "vfproj" || 
            filenameExtention == "vcw") {
          mParentWizard->mExistingVisualStudioList.append(tempFilename);
        }
        
        //cmake file
        if (filenameExtention == "json") {
          if (filenameInfo.fileName() == "compile_commands.json") {
            mParentWizard->mExistingJsonList.append(tempFilename);
          }
        }
        
        // cmake misc files found
        if (!mParentWizard->mHasCMakeFiles && filenameExtention == "txt") {
          if (filenameInfo.fileName().toLower() == "cmakelists.txt") {
            mParentWizard->mHasCMakeFiles = true;
          }
        }
       
        // every other specified extension
        if (mParentWizard->mFileExtensions.contains(filenameExtention)) {
          if (mParentWizard->mSearchContents.contains(filenameExtention)) {
            extentionValue = mParentWizard->mSearchContents.value(filenameExtention);
          }
          extentionValue++;
          mParentWizard->mSearchContents.insert(filenameExtention, extentionValue);
          mParentWizard->mDirectoryList << tempFilename;
        }
      }
    }
    
    numberOfProcessedFiles = mParentWizard->mDirectoryList.count();
    
    foreach(QString tempDirName, tempDirNames) {
      foreach(QString testDirName, tempDirNames) {
        if (tempDirName == testDirName) { // same name do not compare
          // do nothing
        } else if (testDirName.startsWith(tempDirName)) {// there is higher level directory to be used
          dirNamesToUse.removeAll(testDirName);
        }
      }
    }
    
    QStringList listOfDirs;
    QStringList listOfFiles;
    foreach (QString dirNameToUse, dirNamesToUse) {
      QDirIterator it(dirNameToUse, QDir::Files, QDirIterator::Subdirectories);//STI::FileUtils::GetFileDialogFileFilters()
      while (it.hasNext() && !breakout) {
        QString itString = it.next();
        QFileInfo itInfo(itString);
        QString itExtention = itInfo.suffix();
        int extentionValue = 0;
        
        // Understand extension
        if (itExtention == "udb" || itExtention == "udx") {
          mParentWizard->mExistingUdbList.append(itString);
        }
        
        // Visual Studio extensions
        if (itExtention == "csproj" || 
            itExtention == "dsp" || 
            itExtention == "dsw" || 
            itExtention == "sln" || 
            itExtention == "vcp" || 
            itExtention == "vcproj" ||
            itExtention == "vbproj" ||
            itExtention == "vcxproj" ||
            itExtention == "vfproj" || 
            itExtention == "vcw") {
          mParentWizard->mExistingVisualStudioList.append(itString);
        }
        
        //cmake file
        if (itExtention == "json") {
          if (itInfo.fileName() == "compile_commands.json") {
            mParentWizard->mExistingJsonList.append(itString);
          }
        }
        
        // cmake misc files found
        if (!mParentWizard->mHasCMakeFiles && itExtention == "txt") {
          if (itInfo.fileName().toLower() == "cmakelists.txt") {
            mParentWizard->mHasCMakeFiles = true;
          }
        }

        // every other specified extension
        if (mParentWizard->mFileExtensions.contains(itExtention)) {        
          if (mParentWizard->mSearchContents.contains(itExtention)) {
            extentionValue = mParentWizard->mSearchContents.value(itExtention);
          }
          extentionValue++;
          mParentWizard->mSearchContents.insert(itExtention, extentionValue);
          mParentWizard->mDirectoryList << itString;
        }
        
        if (mProgressDialog->wasCanceled()) {
          breakout = true;
        }
        
        if (numberOfProcessedFiles % 756 == 0)
				{
          mProgressDialog->setLabelText(tr("Processed files: %1").arg(numberOfProcessedFiles));
          mProgressDialog->setValue(numberOfProcessedFiles);
        }

        ++numberOfProcessedFiles;
      }
    }
    
    if (!breakout) {
      QMap<QString, STI::Settings::FileTypes::Type>::const_iterator e = mParentWizard->mFileTypes.constBegin();
      while (e != mParentWizard->mFileTypes.constEnd()) {
        if (e.value() == STI::Settings::FileTypes::Cpp) {
          QString extensionString = e.key();
          extensionString.replace(".","");
          if (mParentWizard->mSearchContents.contains(extensionString.toLower()) || mParentWizard->mSearchContents.contains(extensionString.toUpper())) {
            mParentWizard->mHasCPPFiles = true;
            break;
          }
        }
        ++e;
      }
    }
    //
    // Close the progress dialog
    mProgressDialog->close();

    
    foreach (QString projectFile, mParentWizard->mExistingUdbList) {
      QFileInfo projectInfo(projectFile);
      QString projectFileSuffix = projectInfo.suffix();
      if (projectInfo.suffix() == "udx") {
        QString udbFilename = projectFile;
        udbFilename.replace("udx", "udb");
        if (mParentWizard->mExistingUdbList.contains(udbFilename)) {
          mParentWizard->mExistingUdbList.removeAll(projectFile);
        }
      }
    }
  }
  
  // if progress was canceled, clear all settings and return false;
  if (breakout) {
    mParentWizard->clearAllSettings();
    return false;
  }
  //allow for no files
  return true;
}

int SourceCodePage::nextId() const
{
  if (mParentWizard->mExistingUdbList.count() > 0) {
    return NewWizard::Page_ProjectFile;
  } else if (mParentWizard->mHasCMakeFiles && !mParentWizard->mExistingJsonList.count()){
    return NewWizard::Page_MissingCMakeImport;
  } else if (mParentWizard->mExistingVisualStudioList.count() || mParentWizard->mExistingJsonList.count()){
    return NewWizard::Page_Import;
  } else if (mParentWizard->mHasCPPFiles) {
    return NewWizard::Page_StrictDetails;
  } 
  return NewWizard::Page_Conclusion;
}

ProjectFilePage::ProjectFilePage(NewWizard * wizardParent)
  : QWizardPage(wizardParent), mParentWizard(wizardParent)
{
  setTitle(tr("Open Existing <i>Understand</i>&trade; Project?"));
  topLabel = new QLabel(tr("It looks like someone already created an Understand project in this directory, would you like to open it? <a href='www.google.com'>More info</a>"));
  topLabel->setWordWrap(true);

  topLabel->setToolTip(tr("We found the following Understand project(s) while analyzing the specified directories. Lucky you, it looks like someone else may have already set everything up. The project may contain everything you want to analyze, open it to explore your code."));
  connect(topLabel, &QLabel::linkActivated, [this]
  {
    mParentWizard->showHelp();
  });

  projectFileListWidget = new QListWidget(this);
  //projectFileListWidget->setUniformItemSizes(true);
  projectFileListWidget->setResizeMode(QListView::Adjust);
  projectFileListWidget->setSortingEnabled(false);
  projectFileListWidget->setTextElideMode(Qt::ElideMiddle);
  //projectFileListWidget->setSpacing(2);
  projectFileListWidget->setProperty("type", 1);
  projectFileListWidget->setFrameShape(QFrame::NoFrame);
  projectFileListWidget->setFocusPolicy(Qt::NoFocus);

  projectFileListWidget->setSelectionMode(QAbstractItemView::SingleSelection);
  
  connect(projectFileListWidget, &QListWidget::itemDoubleClicked, [this](QListWidgetItem *item) {
    mParentWizard->mProjectName = item->data(Qt::UserRole).toString();
    mParentWizard->openProjectWizard();
  });

  
  connect(projectFileListWidget, &QListWidget::currentItemChanged,
          this, &ProjectFilePage::HandleSelectionChanged);

  QVBoxLayout *layout = new QVBoxLayout;
  layout->addWidget(topLabel);
  layout->addWidget(projectFileListWidget);
  setLayout(layout);
    
}


void ProjectFilePage::resizeEvent(QResizeEvent *event)
{
  QWidget::resizeEvent(event);

  // if the viewport is not reporting a valid size, use the widget size directly
  int widgetSize = projectFileListWidget->viewport()->width() - 11;
  if (widgetSize < 400) {
    widgetSize = projectFileListWidget->size().width() - 31;
  }
  
  foreach(QListWidgetItem * itemWidget, projectFileListWidget->findItems("*",Qt::MatchWildcard)) {
    QWidget * sourceWidget = projectFileListWidget->itemWidget(itemWidget);
    QList<QWidget*> widgetChildren = sourceWidget->findChildren<QWidget*>();          
    foreach(QWidget* childWidget, widgetChildren) {
      QString widgetName = childWidget->metaObject()->className();
      if (widgetName == "QLabel") {
        QLabel * testLabel = static_cast<QLabel *>(childWidget);
        if (testLabel->objectName() ==  "projectfilename") {
          QString fullString = itemWidget->data(Qt::UserRole).toString();
          QString newElideString;
          // Elid in the center...
          QFontMetrics fMetrics = fontMetrics();
          newElideString = fMetrics.elidedText(fullString, Qt::ElideMiddle, widgetSize - projectButtonWidth);
          testLabel->setText(newElideString);
        }
      }
    }
  }
}

void ProjectFilePage::HandleSelectionChanged(QListWidgetItem *current,QListWidgetItem *previous)
{
  // if the viewport is not reporting a valid size, use the widget size directly
  int widgetSize = projectFileListWidget->viewport()->width() - 11;
  if (widgetSize < 400) {
    widgetSize = projectFileListWidget->size().width() - 31;
  }
  
  QWidget* currentWidget = projectFileListWidget->itemWidget(current);
  if (currentWidget) {
    QList<QWidget*> widgetChildren = currentWidget->findChildren<QWidget*>();          
    foreach(QWidget* childWidget, widgetChildren) {
      QString widgetName = childWidget->metaObject()->className();
      //if (widgetName == "QToolButton") {
      //  QToolButton * testToolButton = static_cast<QToolButton *>(childWidget);
      //  testToolButton->show();
      //} else 
      if (widgetName == "QLabel") {
        QLabel * testLabel = static_cast<QLabel *>(childWidget);
        if (testLabel->objectName() ==  "projectfilename") {
          QString fullString = current->data(Qt::UserRole).toString();
          QString newElideString;
          // Elid in the center...
          QFontMetrics fMetrics = fontMetrics();
          newElideString = fMetrics.elidedText(fullString, Qt::ElideMiddle, widgetSize - projectButtonWidth);
          testLabel->setText(newElideString);
        }
      }
    }
  }
  
  QWidget * sourceWidget = projectFileListWidget->itemWidget(previous);
  if (sourceWidget) {
    QList<QWidget*> widgetChildren = sourceWidget->findChildren<QWidget*>();          
    foreach(QWidget* childWidget, widgetChildren) {
      QString widgetName = childWidget->metaObject()->className();
      //if (widgetName == "QToolButton") {
      //  QToolButton * testToolButton = static_cast<QToolButton *>(childWidget);
      //  testToolButton->hide();
      //} else 
      if (widgetName == "QLabel") {
        QLabel * testLabel = static_cast<QLabel *>(childWidget);
        if (testLabel->objectName() ==  "projectfilename") {
          QString fullString = previous->data(Qt::UserRole).toString();
          QString newElideString;
          // Elid in the center...
          QFontMetrics fMetrics = fontMetrics();
          newElideString = fMetrics.elidedText(fullString, Qt::ElideMiddle, widgetSize - projectButtonWidth);
          testLabel->setText(newElideString);
        }
      }
    }
  }
}

bool ProjectFilePage::validatePage() 
{
  return true;
}

void ProjectFilePage::initializePage() 
{
  mParentWizard->mPathTaken = NewWizard::Path_Standard;
  projectFileListWidget->clear();
  
  //projectFileListWidget->addItems(mParentWizard->mExistingUdbList);
  foreach (QString existingUdbFile, mParentWizard->mExistingUdbList) {
    QListWidgetItem * newItem = new QListWidgetItem();
    newItem->setToolTip(existingUdbFile);
    newItem->setData(Qt::UserRole, existingUdbFile);

    QWidget* newWidget = new QWidget();
    QHBoxLayout * hBoxLayout = new QHBoxLayout(newWidget);
    
    QToolButton * newButton = new QToolButton();
    newButton->setAutoRaise(false);
    newButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    newButton->setToolTip(tr("Open Project"));
    newButton->setText(tr("Open"));
    newButton->setIcon(Icon(":/icons/projectopen.svg"));
    connect(newButton, &QToolButton::clicked, [this, newItem] {
      mParentWizard->mProjectName = newItem->data(Qt::UserRole).toString();
      mParentWizard->openProjectWizard();
    });

    projectButtonWidth = newButton->sizeHint().width();

    // if the viewport is not reporting a valid size, use the widget size directly
    int widgetSize = projectFileListWidget->viewport()->width() - 11;
    if (widgetSize < 400) {
      widgetSize = projectFileListWidget->size().width() - 31;
    }

    QString newString = newItem->data(Qt::UserRole).toString();
    QString newElideString;
    // Elid in the center...
    QFontMetrics fMetrics = fontMetrics();
    newElideString = fMetrics.elidedText(newString, Qt::ElideMiddle, widgetSize - projectButtonWidth);

    //QFileInfo fileInfo(newString);
    //QLabel * fileLabel = new QLabel(fileInfo.fileName());
    //QLabel * fileDate = new QLabel(fileInfo.lastModified().toString(Qt::SystemLocaleShortDate));

    QLabel * textLabel = new QLabel(newElideString);
    textLabel->setObjectName("projectfilename");

    hBoxLayout->addWidget(textLabel, 0);
    hBoxLayout->addItem(new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum));
    hBoxLayout->addWidget(newButton, 0);
    hBoxLayout->addSpacing(2);

    hBoxLayout->setSpacing(2);
    hBoxLayout->setMargin(2);

    newWidget->setLayout(hBoxLayout);
    newWidget->setContentsMargins(0,0,0,0);

    newItem->setSizeHint(newWidget->sizeHint());

    projectFileListWidget->addItem(newItem);
    projectFileListWidget->setItemWidget(newItem, newWidget);
  }
  projectFileListWidget->setCurrentRow(projectFileListWidget->count() - 1);
  projectFileListWidget->setCurrentRow(0);
  mParentWizard->idChanged(NewWizard::Page_ProjectFile);
}

int ProjectFilePage::nextId() const
{
  if (mParentWizard->mHasCMakeFiles && !mParentWizard->mExistingJsonList.count()){
    return NewWizard::Page_MissingCMakeImport;
  } else if (mParentWizard->mExistingVisualStudioList.count() || mParentWizard->mExistingJsonList.count()){
    return NewWizard::Page_Import;
  } else if (mParentWizard->mHasCPPFiles) {
    return NewWizard::Page_StrictDetails;
  } 
  return NewWizard::Page_Conclusion;
}

MissingCMakeImportPage::MissingCMakeImportPage(NewWizard * wizardParent)
  : QWizardPage(wizardParent), mParentWizard(wizardParent)
{
  setTitle(tr("Specify CMake Compilation Database?"));
  //setPixmap(QWizard::BackgroundPixmap, QPixmap(":/images/understandWizard.png"));
  
  topLabel = new QLabel(tr("We notice this project uses CMake but we did not find a compilation database file. "
                           "This file (command_compile.json) will allow for a <i><Strong>much more accurate</Strong></i> Understand project. \n\n"
                           "Would you like to specify a compilation database file (instructions below)?"));
  topLabel->setWordWrap(true);

  missingCMakeAddFileButton = new QToolButton(this);
  
  missingCMakeAddFileButton->setText(tr("Add File"));
  
  bottomLabel = new QLabel(tr("How To Create a CMake Compilation Database:"));
  bottomLabel->setWordWrap(true);
  bottomInstructions = new QLabel(tr("1. First, find and open the file CMakeCache.txt in the root of your build folder (i.e. C:/Code/build/release/CMakeCache.txt).\n"
                                      "2. Change the value for CMAKE_EXPORT_COMPILE_COMMANDS to 'On' and save and close the file.\n"
                                      "3. Run the clean build target (e.g. 'make clean' or 'ninja clean')\n"
                                      "4. Run the build target for the desired project (e.g. 'make myProject' or 'ninja myProject')\n"
                                      "5. The file compile_commands.json should now show in the root of your build folder (i.e. C:/code/build/release/compile_commands.json)\n"));
  bottomInstructions->setWordWrap(true);
  bottomInstructions->setEnabled(false);
  
  connect(mParentWizard->button(QWizard::CustomButton3), &QAbstractButton::clicked, [this]() {
    importProjectWizard();
  });
  
  connect(missingCMakeAddFileButton, &QToolButton::clicked, [this]() {
    QFileDialog * dialog = new QFileDialog(this,tr("Choose a CMake File"));
    
    dialog->setFileMode(QFileDialog::ExistingFile);
    dialog->setOptions(QFileDialog::DontResolveSymlinks);
    dialog->setNameFilter(tr("CMake (compile_commands.json)"));

    QStringList cmakeFileNames;
    if (dialog->exec()) {
      cmakeFileNames = dialog->selectedFiles();
    }   

    if (!cmakeFileNames.isEmpty()) {
      mParentWizard->mImportName = cmakeFileNames.takeFirst();
      importProjectWizard();
    }
  });
  
  QHBoxLayout * topButtonLayout = new QHBoxLayout;
  topButtonLayout->addWidget(missingCMakeAddFileButton);
  topButtonLayout->addItem(new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum));
  
  QVBoxLayout * bottomLayout = new QVBoxLayout();
  bottomLayout->addWidget(bottomLabel);
 
  QHBoxLayout * labelLayout = new QHBoxLayout();
  labelLayout->addSpacing(20);
  labelLayout->addWidget(bottomInstructions);

  QVBoxLayout *layout = new QVBoxLayout;
  layout->addWidget(topLabel);
  layout->addLayout(topButtonLayout);
  layout->addLayout(bottomLayout);
  layout->addLayout(labelLayout);
  setLayout(layout);
}


void MissingCMakeImportPage::initializePage() 
{
  mParentWizard->mPathTaken = NewWizard::Path_Standard;
  mParentWizard->mImportName.clear();
  importButtonPressed = false;
  
}

bool MissingCMakeImportPage::validatePage() 
{
  //allow for no files
  return true;
}

void MissingCMakeImportPage::importProjectWizard() 
{
  importButtonPressed = true;
  mParentWizard->next();
  importButtonPressed = false;
}

int MissingCMakeImportPage::nextId() const
{
  mParentWizard->mPathTaken = NewWizard::Path_Standard;
  if (importButtonPressed && mParentWizard->mImportName.endsWith(".json")) {
    mParentWizard->mPathTaken = NewWizard::Path_ImportCMake;
    mParentWizard->mStrictRadioButton->setChecked(true);
    mParentWizard->mFuzzyRadioButton->setChecked(false);
    //return NewWizard::Page_ImportJsonDetails;
    return NewWizard::Page_Conclusion;
  } else if (mParentWizard->mExistingVisualStudioList.count() || mParentWizard->mExistingJsonList.count()){
    return NewWizard::Page_Import;
  } else if (mParentWizard->mHasCPPFiles) {
    return NewWizard::Page_StrictDetails;
  } 
  return NewWizard::Page_Conclusion;
}

ImportPage::ImportPage(NewWizard * wizardParent)
  : QWizardPage(wizardParent), mParentWizard(wizardParent)
{
  setTitle(tr("Import Settings from Other Tools?"));
  topLabel = new QLabel(tr("We found these project files from other tools that we can use to setup your project. "
                           "This can provide an <i><Strong>easier setup and more accurate result</Strong></i> than analyzing the entire directory. "
                           "Would you like to base your project on one of these? <a href='www.scitools.com'>More info</a>"));

  topLabel->setWordWrap(true);

  connect(topLabel, &QLabel::linkActivated, [this]
  {
    mParentWizard->showHelp();
  });

  importListWidget = new QListWidget(this);
  importListWidget->setResizeMode(QListView::Adjust);
  importListWidget->setSortingEnabled(false);
  importListWidget->setTextElideMode(Qt::ElideMiddle);
  importListWidget->setSelectionMode(QAbstractItemView::SingleSelection);
  importListWidget->setProperty("type", 1);
  importListWidget->setFrameShape(QFrame::NoFrame);
  importListWidget->setFocusPolicy(Qt::NoFocus);
  importListWidget->setSpacing(2);

  QHBoxLayout *searchLayout = new QHBoxLayout();
  mParentWizard->mNewWizardSearch = new NewWizardSearch(this);
  mParentWizard->mNewWizardSearch->setListWidget(importListWidget);
  searchLayout->addSpacing(10);
  //mSettingsConfigSearch = new OptionsSearch(pagesTree, true, this);
  searchLayout->addWidget(mParentWizard->mNewWizardSearch);
  
  mParentWizard->mNewWizardSearch->show();
  mParentWizard->mNewWizardSearch->executeSearch();

  QVBoxLayout *layout = new QVBoxLayout;
  layout->addWidget(topLabel);
  layout->addLayout(searchLayout);
  layout->addWidget(importListWidget);
  setLayout(layout);

  connect(importListWidget, &QListWidget::itemDoubleClicked, [this](QListWidgetItem *item) {
    mParentWizard->mImportName = item->data(Qt::UserRole).toString();
    importProjectWizard();
  });
  
  connect(importListWidget, &QListWidget::currentItemChanged,
          this, &ImportPage::HandleSelectionChanged);
}

void ImportPage::HandleSelectionChanged(QListWidgetItem *current,QListWidgetItem *previous)
{
  // if the viewport is not reporting a valid size, use the widget size directly
  int widgetSize = importListWidget->viewport()->width() - 11;
  if (widgetSize < 400) {
    widgetSize = importListWidget->size().width() - 31;
  }

  QWidget* currentWidget = importListWidget->itemWidget(current);
  if (currentWidget) {
    QList<QWidget*> widgetChildren = currentWidget->findChildren<QWidget*>();          
    foreach(QWidget* childWidget, widgetChildren) {
      QString widgetName = childWidget->metaObject()->className();
      //if (widgetName == "QToolButton") {
      //  QToolButton * testToolButton = static_cast<QToolButton *>(childWidget);
      //  testToolButton->show();
      //} else 
        if (widgetName == "QLabel") {
        QLabel * testLabel = static_cast<QLabel *>(childWidget);
        if (testLabel->objectName() ==  "importfilename") {
          QString fullString = current->data(Qt::UserRole).toString();
          QString newElideString;
          // Elid in the center...
          QFontMetrics fMetrics = fontMetrics();
          newElideString = fMetrics.elidedText(fullString, Qt::ElideMiddle, widgetSize);
          testLabel->setText(newElideString);
        }
      }
    }
  }
  
  QWidget * sourceWidget = importListWidget->itemWidget(previous);
  if (sourceWidget) {
    QList<QWidget*> widgetChildren = sourceWidget->findChildren<QWidget*>();          
    foreach(QWidget* childWidget, widgetChildren) {
      QString widgetName = childWidget->metaObject()->className();
      //if (widgetName == "QToolButton") {
      //  QToolButton * testToolButton = static_cast<QToolButton *>(childWidget);
      //  testToolButton->hide();
      //} else 
        if (widgetName == "QLabel") {
        QLabel * testLabel = static_cast<QLabel *>(childWidget);
        if (testLabel->objectName() ==  "importfilename") {
          QString fullString = previous->data(Qt::UserRole).toString();
          QString newElideString;
          // Elid in the center...
          QFontMetrics fMetrics = fontMetrics();
          newElideString = fMetrics.elidedText(fullString, Qt::ElideMiddle, widgetSize);
          testLabel->setText(newElideString);
        }
      }
    }
  }
  
  importButtonPressed = false;
}

void ImportPage::resizeEvent(QResizeEvent *event)
{
  QWidget::resizeEvent(event);

  // if the viewport is not reporting a valid size, use the widget size directly
  int widgetSize = importListWidget->viewport()->width() - 16;
  if (widgetSize < 400) {
    widgetSize = importListWidget->size().width() - 36;
  }
  
  foreach(QListWidgetItem * itemWidget, importListWidget->findItems("*",Qt::MatchWildcard)) {
    QWidget * sourceWidget = importListWidget->itemWidget(itemWidget);
    QList<QWidget*> widgetChildren = sourceWidget->findChildren<QWidget*>();          
    foreach(QWidget* childWidget, widgetChildren) {
      QString widgetName = childWidget->metaObject()->className();
      if (widgetName == "QLabel") {
        QLabel * testLabel = static_cast<QLabel *>(childWidget);
        if (testLabel->objectName() ==  "importfilename") {
          QString fullString = itemWidget->data(Qt::UserRole).toString();
          QString newElideString;
          // Elid in the center...
          QFontMetrics fMetrics = fontMetrics();
          newElideString = fMetrics.elidedText(fullString, Qt::ElideMiddle, widgetSize);
          testLabel->setText(newElideString);
        }
      }
    }
  }
}

int ImportPage::nextId() const
{
  mParentWizard->mPathTaken = NewWizard::Path_Standard;
  if (importButtonPressed) {
    if (!mParentWizard->mImportName.endsWith(".json")) {
      return NewWizard::Page_ImportVisualStudioDetails;
    } else {
      mParentWizard->mPathTaken = NewWizard::Path_ImportCMake;
      mParentWizard->mStrictRadioButton->setChecked(true);
      mParentWizard->mFuzzyRadioButton->setChecked(false);
      //return NewWizard::Page_ImportJsonDetails;
      return NewWizard::Page_Conclusion;
    }
  } else if (mParentWizard->mHasCPPFiles) {
    return NewWizard::Page_WatchBuild;
    //return NewWizard::Page_StrictDetails;
  } 
  return NewWizard::Page_Conclusion;
}

void ImportPage::initializePage() 
{
  mParentWizard->mPathTaken = NewWizard::Path_Standard;
  importListWidget->clear();
  importButtonPressed = false;
  mParentWizard->mHasImportCPPFiles = false;
  
  // if the viewport is not reporting a valid size, use the widget size directly
  int widgetSize = importListWidget->viewport()->width() - 11;
  if (widgetSize < 400) {
    widgetSize = importListWidget->size().width() - 31;
  }

  for (int i = 0; i < mParentWizard->mExistingJsonList.count(); i++) {
    QString newItemText = mParentWizard->mExistingJsonList.at(i);
    QListWidgetItem * newItem = new QListWidgetItem();
    newItem->setData(Qt::UserRole, newItemText);

    QWidget* newWidget = new QWidget();
    QHBoxLayout * hBoxLayout = new QHBoxLayout();
    QVBoxLayout * vBoxLayout = new QVBoxLayout(newWidget);
    QHBoxLayout * hBoxLayout1 = new QHBoxLayout();
    
    QToolButton * newButton = new QToolButton(this);
    newButton->setAutoRaise(false);
    newButton->setText(tr("Import"));
    newButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    newButton->setToolTip(tr("Import File"));
    newButton->setIcon(Icon(":/icons/import-config-file.svg"));
    connect(newButton, &QToolButton::clicked, [this, newItem] {
      mParentWizard->mImportName = newItem->data(Qt::UserRole).toString();
      importProjectWizard();
    });

    QString newString = newItem->data(Qt::UserRole).toString();
    QString newElideString;
    // Elid in the center...
    QFontMetrics fMetrics = fontMetrics();
    newElideString = fMetrics.elidedText(newString, Qt::ElideMiddle, widgetSize);

    QFileInfo fileInfo(newString);
    QLabel * fileLabel = new QLabel(fileInfo.fileName());
    fileLabel->setToolTip(newString);
    fileLabel->setProperty("type", 1);
    fileLabel->setObjectName("filename");
    this->style()->unpolish(fileLabel);
    this->style()->polish(fileLabel);
    QLabel * fileDateLabel = new QLabel(fileInfo.lastModified().toString("yyyy-MM-dd   hh:mm:ss"));
    fileDateLabel->setToolTip(tr("Last Modified Date"));
    fileDateLabel->setProperty("type", 1);
    fileDateLabel->setObjectName("modifieddate");
    this->style()->unpolish(fileDateLabel);
    this->style()->polish(fileDateLabel);
    QLabel * fileTypeLabel = new QLabel(tr("CMake File"));
    fileTypeLabel->setToolTip(tr("File Type"));
    //fileTypeLabel->setProperty("type", 1);
    fileTypeLabel->setObjectName("filetype");
    //this->style()->unpolish(fileTypeLabel);
    //this->style()->polish(fileTypeLabel);
    QLabel * textLabel = new QLabel(newElideString);
    textLabel->setObjectName("importfilename");
    textLabel->setToolTip(newItemText);

    hBoxLayout1->addWidget(fileLabel);
    hBoxLayout1->addItem(new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum));
    hBoxLayout1->addWidget(fileDateLabel);
    hBoxLayout1->addSpacing(2);

    
    hBoxLayout->addWidget(fileTypeLabel, 0);
    hBoxLayout->addItem(new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum));
    hBoxLayout->addWidget(newButton, 0);
    hBoxLayout->addSpacing(2);

    hBoxLayout->setSpacing(2);
    hBoxLayout->setMargin(2);
    hBoxLayout1->setSpacing(2);
    hBoxLayout1->setMargin(2);
    vBoxLayout->setSpacing(0);
    vBoxLayout->setMargin(2);

    vBoxLayout->addLayout(hBoxLayout1);
    vBoxLayout->addLayout(hBoxLayout);
    vBoxLayout->addWidget(textLabel);
    
    newWidget->setLayout(vBoxLayout);
    newWidget->setContentsMargins(0,0,0,0);

    newItem->setSizeHint(newWidget->sizeHint());

    importListWidget->addItem(newItem);
    importListWidget->setItemWidget(newItem, newWidget);
  }
  
  for (int i = 0; i < mParentWizard->mExistingVisualStudioList.count(); i++) {
    QString newItemText = mParentWizard->mExistingVisualStudioList.at(i);
    QListWidgetItem * newItem = new QListWidgetItem();
    newItem->setToolTip(newItemText);
    newItem->setData(Qt::UserRole, newItemText);

    QWidget* newWidget = new QWidget();
    QHBoxLayout * hBoxLayout = new QHBoxLayout();
    QVBoxLayout * vBoxLayout = new QVBoxLayout(newWidget);
    QHBoxLayout * hBoxLayout1 = new QHBoxLayout();
    
    QToolButton * newButton = new QToolButton(this);
    newButton->setAutoRaise(false);
    newButton->setText(tr("Import"));
    newButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    newButton->setToolTip(tr("Import File"));
    newButton->setIcon(Icon(":/icons/import-config-file.svg"));
    connect(newButton, &QToolButton::clicked, [this, newItem] {
      mParentWizard->mImportName = newItem->data(Qt::UserRole).toString();
      importProjectWizard();
    });

    QString newString = newItem->data(Qt::UserRole).toString();
    QString newElideString;
    // Elid in the center...
    QFontMetrics fMetrics = fontMetrics();
    newElideString = fMetrics.elidedText(newString, Qt::ElideMiddle, widgetSize);

    QFileInfo fileInfo(newString);
    QLabel * fileLabel = new QLabel(fileInfo.fileName());
    fileLabel->setToolTip(newString);
    fileLabel->setProperty("type", 1);
    fileLabel->setObjectName("filename");
    this->style()->unpolish(fileLabel);
    this->style()->polish(fileLabel);
    QLabel * fileDateLabel = new QLabel(fileInfo.lastModified().toString("yyyy-MM-dd   hh:mm:ss"));
    fileDateLabel->setToolTip(tr("Last Modified Date"));
    fileDateLabel->setProperty("type", 1);
    fileDateLabel->setObjectName("modifieddate");
    this->style()->unpolish(fileDateLabel);
    this->style()->polish(fileDateLabel);
    QLabel * fileTypeLabel = new QLabel(tr("Visual Studio File"));
    fileTypeLabel->setToolTip(tr("File Type"));
    //fileTypeLabel->setProperty("type", 1);
    fileTypeLabel->setObjectName("filetype");
    //this->style()->unpolish(fileTypeLabel);
    //this->style()->polish(fileTypeLabel);
    QLabel * textLabel = new QLabel(newElideString);
    textLabel->setObjectName("importfilename");
    textLabel->setToolTip(newItemText);

    hBoxLayout1->addWidget(fileLabel);
    hBoxLayout1->addItem(new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum));
    hBoxLayout1->addWidget(fileDateLabel);
    hBoxLayout1->addSpacing(2);

    
    hBoxLayout->addWidget(fileTypeLabel, 0);
    hBoxLayout->addItem(new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum));
    hBoxLayout->addWidget(newButton, 0);
    hBoxLayout->addSpacing(2);

    hBoxLayout->setSpacing(2);
    hBoxLayout->setMargin(2);
    hBoxLayout1->setSpacing(2);
    hBoxLayout1->setMargin(2);
    vBoxLayout->setSpacing(0);
    vBoxLayout->setMargin(2);

    vBoxLayout->addLayout(hBoxLayout1);
    vBoxLayout->addLayout(hBoxLayout);
    vBoxLayout->addWidget(textLabel);
    
    newWidget->setLayout(vBoxLayout);
    newWidget->setContentsMargins(0,0,0,0);

    newItem->setSizeHint(newWidget->sizeHint());

    importListWidget->addItem(newItem);
    importListWidget->setItemWidget(newItem, newWidget);
  }
  
  importListWidget->setCurrentRow(importListWidget->count() - 1);
  importListWidget->setCurrentRow(0);
}

void ImportPage::importProjectWizard() 
{
  importButtonPressed = true;
  mParentWizard->next();
  importButtonPressed = false;
}

ImportVisualStudioDetailsPage::ImportVisualStudioDetailsPage(NewWizard * wizardParent)
  : QWizardPage(wizardParent), mParentWizard(wizardParent)
{
  setTitle(tr("Specify Visual Studio Configuration"));

  topLabel = new QLabel(tr("Visual Studio projects can have multiple configurations while Understand can only display one state at a time. "
                           "Please select the configuration state you would like for this project. <a href='www.google.com'>More info</a>"));
  topLabel->setWordWrap(true);

  topLabel->setToolTip(tr("The configuration state determines what files to include in the project and what macro values are set. "));

  connect(topLabel, &QLabel::linkActivated, [this]
  {
    mParentWizard->showHelp();
  });
  
  bottomLabel = new QLabel(tr("<a href='www.google.com'>More info</a>"));
  QString bottomText = tr("Normally the Understand project stays synchronized with the Visual Studio project, "
                          "so if a file is added to the MSVC project it will automatically be added to the Understand project. "
                          "Check this option if you do not want them synchronized. "
                          "We will still create your initial Understand project with the same settings as the Visual Studio project.");
  QString bottomTextWithHeader = tr("<i><Strong>One Time Import</Strong></i><br><br>%1").arg(bottomText);
  bottomLabel->setToolTip(bottomText);
  connect(bottomLabel, &QLabel::linkActivated, [this, bottomTextWithHeader]
  {
    QMessageBox::information(this, tr("New Project Wizard Help"), bottomTextWithHeader);
  });
  
  mVisualStudioTreeWidget = new QTreeWidget(this);
  mVisualStudioTreeWidget->setHeaderHidden(true);
  mVisualStudioTreeWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
  mVisualStudioTreeWidget->setSelectionMode(QAbstractItemView::ContiguousSelection);
  mVisualStudioTreeWidget->setEnabled(false);
  mVisualStudioTreeWidget->setSelectionMode(QAbstractItemView::NoSelection);
  mVisualStudioTreeWidget->setFocusPolicy(Qt::NoFocus);


  QHBoxLayout * configurationBoxLayout = new QHBoxLayout();
  configurationBoxLayout->addWidget(mParentWizard->mConfigurationComboBox);
  configurationBoxLayout->addItem(new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum));
  
  mFormLayout = new QFormLayout(this);
  mFormLayout->addRow(tr("Configuration:"),configurationBoxLayout);
  mFormLayout->addRow(mVisualStudioTreeWidget);
  setLayout(mFormLayout);
  
  connect(mParentWizard->mConfigurationComboBox, SIGNAL(currentIndexChanged(const QString &)), this, SLOT(CurrentIndexChanged(const QString &)));

}

void ImportVisualStudioDetailsPage::initializePage()
{
  mParentWizard->mConfigurationComboBox->clear();
  mVisualStudioTreeWidget->clear();
  mParentWizard->mPathTaken = NewWizard::Path_ImportVisualStudio;
  
  QString newString = mParentWizard->mImportName;
  QString newElideString;
  // Elid in the center...
  QFontMetrics fMetrics = fontMetrics();
  newElideString = fMetrics.elidedText(newString, Qt::ElideMiddle, mFormLayout->sizeHint().width());
  ConfigurationTextChanged(newString);
}

void ImportVisualStudioDetailsPage::CurrentIndexChanged(
  const QString &text)
{
  mVisualStudioTreeWidget->setEnabled(!text.isEmpty());
  QString vs = mParentWizard->mImportName;
  QString config = mParentWizard->mConfigurationComboBox->currentText();

  mVisualStudioTreeWidget->clear();
  AddFile(vs,config);
  mVisualStudioTreeWidget->resizeColumnToContents(0); //need to force update it somehow, possible new qt bug
}

void ImportVisualStudioDetailsPage::ConfigurationTextChanged(
  const QString &text)
{
  mParentWizard->mConfigurationComboBox->clear();

  // cleanup filename
  QString filename = QDir::toNativeSeparators(QFileInfo(text).absoluteFilePath());

  if (QFileInfo(filename).isAbsolute()) {
    VisualStudio::Solution::Status status;
    VisualStudio::Solution *solution = VisualStudio::Solution::parse(filename,status);

    // get list of configurations from solution
    QStringList configurations;
    if (status == VisualStudio::Solution::Okay && solution != 0) {
      // use solution configurations
      if (solution->supportsConfigurations()) {
        configurations = solution->listConfigurations();
      }
      // use configurations from each contained project
      else {
        foreach (QString project,solution->listProjects()) {
          if (QFileInfo(project).isAbsolute()) {
            VisualStudio::Project::Status status;
            configurations << VisualStudio::Project::parseConfigurations(project,status);
          }
        }
        configurations.removeDuplicates();
      }
      delete solution;
      solution = 0;
    }

    // get list of configurations from project
    else {
      VisualStudio::Project::Status status;
      configurations = VisualStudio::Project::parseConfigurations(filename,status);
    }

    // add configurations
    if (configurations.count()) {
      for (int i=0; i<configurations.count(); ++i)
        mParentWizard->mConfigurationComboBox->addItem(configurations.at(i));
        mParentWizard->mConfigurationComboBox->setEnabled(true);
    }
  }
  // get the minimum width that fits the largest item.
  int width = mParentWizard->mConfigurationComboBox->minimumSizeHint().width();
  width += 40;
  mParentWizard->mConfigurationComboBox->setMinimumWidth(width);
}

void ImportVisualStudioDetailsPage::CurrentChanged() {
  QString filename = mParentWizard->mImportName;
  QString config = mParentWizard->mConfigurationComboBox->currentText();
  VisualStudio::Solution::Status status;
  VisualStudio::Solution *solution = VisualStudio::Solution::parse(filename, status);

  if (status == VisualStudio::Solution::Okay && solution) {
    // handle solution file
    foreach (const QString &project_name, solution->listProjects()) {
      QString project_config = solution->getProjectConfiguration(project_name, config);
      AddProjectFile(project_name, filename, !project_config.isEmpty() ? project_config : config);
    }

    delete solution;
    solution = 0;

  } else {
    // handle project file
    AddProjectFile(filename,"",config);
  }
}

// Add a project file. If the project was part of a solution, specify the
// solution name; otherwise leave it empty.
void ImportVisualStudioDetailsPage::AddProjectFile(
  const QString &project_name,
  const QString &solution_name,
  const QString &config)
{
  mParentWizard->mImportSearchContents.clear();
  mParentWizard->mImportList.clear();
  VisualStudio::Project::Status status;
  VisualStudio::Project *project = VisualStudio::Project::parse(project_name,solution_name,config,status);
  if (status != VisualStudio::Project::Okay)
    return;

  QFont bold = font();
  bold.setBold(true);

  // add root as project name
  QTreeWidgetItem *root = new QTreeWidgetItem(QStringList(project->getProjectFilename()));
  root->setFont(0, bold);
  mVisualStudioTreeWidget->addTopLevelItem(root);

  // project configuration
  if (!config.isEmpty()) {
    QTreeWidgetItem *item = new QTreeWidgetItem(root, QStringList(tr("Configuration: %1").arg(config)));
    item->setFont(0, bold);
  }

  // root namespace (vb)
  if (!project->getRootNamespace().isEmpty()) {
    QTreeWidgetItem *item = new QTreeWidgetItem(root, QStringList(tr("Root Namespace: %1").arg(project->getRootNamespace())));
    item->setFont(0, bold);
  }

  // autoincludes
  QStringList autoincludes = project->listAutoIncludes();
  if (!autoincludes.isEmpty()) {
    QTreeWidgetItem *item = new QTreeWidgetItem(root, QStringList(tr("Autoinclude")));
    item->setFont(0, bold);
    foreach (const QString &autoinclude, autoincludes)
      new QTreeWidgetItem(item, QStringList(autoinclude));
  }

  // includes
  QStringList includes = project->listIncludes();
  if (!includes.isEmpty()) {
    QTreeWidgetItem *item = new QTreeWidgetItem(root, QStringList(tr("Include")));
    item->setFont(0, bold);
    foreach (const QString &include, includes)
      new QTreeWidgetItem(item, QStringList(include));
  }

  // defines
  QStringList defines = project->listDefines();
  if (!defines.isEmpty()) {
    QTreeWidgetItem *item = new QTreeWidgetItem(root, QStringList(tr("Define")));
    item->setFont(0, bold);
    foreach (const QString &define, defines)
      new QTreeWidgetItem(item, QStringList(define));
  }

  // imported namespaces
  QStringList namespaces = project->listNamespaces();
  if (!namespaces.isEmpty()) {
    namespaces.sort();
    QTreeWidgetItem *item = new QTreeWidgetItem(root, QStringList(tr("Imported Namespaces")));
    item->setFont(0, bold);
    foreach (const QString &name, namespaces)
      new QTreeWidgetItem(item, QStringList(name));
  }

  // references
  QMap<QString,QString> refs = project->listReferences();
  if (!refs.isEmpty()) {
    QTreeWidgetItem *item = new QTreeWidgetItem(root, QStringList(tr("References")));
    item->setFont(0, bold);
    for (QMap<QString,QString>::const_iterator pos=refs.constBegin(); pos!=refs.constEnd(); ++pos) {
      new QTreeWidgetItem(item, QStringList(pos.key()));
      if (!pos.value().isEmpty())
        new QTreeWidgetItem(item, QStringList(pos.value()));
    }
  }

  // files
  QList<VisualStudio::File> files = project->listFiles();
  if (!files.isEmpty()) {
    QTreeWidgetItem *filesItem = new QTreeWidgetItem(root, QStringList(tr("Files")));
    filesItem->setFont(0, bold);

    foreach (const VisualStudio::File &file, files) {
      if (file.isExcluded()) {
        QFont italic = font();
        italic.setItalic(true);
        QString text = tr("%1 - %2").arg(file.getFilename(), tr("Excluded"));
        QTreeWidgetItem *fileItem = new QTreeWidgetItem(filesItem, QStringList(text));
        fileItem->setFont(0, italic);
        continue;
      }

      QFileInfo fileInfo(file.getFilename());
      QString fileExtention = fileInfo.suffix();
      int extentionValue = 0;

      // don't filter any files out at this point
      //if (mParentWizard->mFileExtensions.contains(fileExtention)) {
      if (mParentWizard->mImportSearchContents.contains(fileExtention)) {
        extentionValue = mParentWizard->mImportSearchContents.value(fileExtention);
      }
      extentionValue++;
      mParentWizard->mImportSearchContents.insert(fileExtention, extentionValue);
      mParentWizard->mImportList << file.getFilename();
      //}

      QTreeWidgetItem *fileItem = new QTreeWidgetItem(filesItem, QStringList(file.getFilename()));

      // root namespace (vb)
      if (!file.getRootNamespace().isEmpty()) {
        QTreeWidgetItem *item = new QTreeWidgetItem(fileItem, QStringList(tr("Root Namespace: %1").arg(file.getRootNamespace())));
        item->setFont(0, bold);
      }

      // autoincludes
      QStringList autoincludes = file.listAutoIncludes(*project);
      if (!autoincludes.isEmpty()) {
        QTreeWidgetItem *item = new QTreeWidgetItem(fileItem, QStringList(tr("Autoinclude")));
        item->setFont(0, bold);
        foreach (const QString &autoinclude, autoincludes)
          new QTreeWidgetItem(item, QStringList(autoinclude));
      }

      // includes
      QStringList includes = file.listIncludes();
      if (!includes.isEmpty()) {
        QTreeWidgetItem *item = new QTreeWidgetItem(fileItem, QStringList(tr("Include")));
        item->setFont(0, bold);
        foreach (const QString &include, includes)
          new QTreeWidgetItem(item, QStringList(include));
      }

      // defines
      QStringList defines = file.listDefines();
      if (!defines.isEmpty()) {
        QTreeWidgetItem *item = new QTreeWidgetItem(fileItem, QStringList(tr("Define")));
        item->setFont(0, bold);
        foreach (const QString &define, defines)
          new QTreeWidgetItem(item, QStringList(define));
      }

      // imported namespaces
      QStringList namespaces = file.listNamespaces();
      if (!namespaces.isEmpty()) {
        namespaces.sort();
        QTreeWidgetItem *item = new QTreeWidgetItem(fileItem, QStringList(tr("Imported Namespaces")));
        item->setFont(0, bold);
        foreach (const QString &name, namespaces)
          new QTreeWidgetItem(item, QStringList(name));
      }

      // references
      QMap<QString,QString> refs = file.listReferences();
      if (!refs.isEmpty()) {
        QTreeWidgetItem *item = new QTreeWidgetItem(fileItem, QStringList(tr("References")));
        item->setFont(0, bold);
        for (QMap<QString,QString>::const_iterator pos=refs.constBegin(); pos!=refs.constEnd(); ++pos) {
          new QTreeWidgetItem(item, QStringList(pos.key()));
          if (!pos.value().isEmpty())
            new QTreeWidgetItem(item, QStringList(pos.value()));
        }
      }
    }
    //mVisualStudioTreeWidget->expandItem(filesItem);
  }

  mVisualStudioTreeWidget->expandItem(root);
  
  delete project;
  project = 0;
}

// Add a solution or project file.
void ImportVisualStudioDetailsPage::AddFile(
  const QString &filename,
  const QString &config)
{
  // filename must be absolute
  if (!QFileInfo(filename).isAbsolute())
    return;

  VisualStudio::Solution::Status status;
  VisualStudio::Solution *solution = VisualStudio::Solution::parse(filename, status);

  if (status == VisualStudio::Solution::Okay && solution) {
    // handle solution file
    foreach (const QString &project_name, solution->listProjects()) {
      QString project_config = solution->getProjectConfiguration(project_name, config);
      AddProjectFile(project_name, filename, !project_config.isEmpty() ? project_config : config);
    }

    delete solution;
    solution = 0;

  } else {
    // handle project file
    AddProjectFile(filename,"",config);
  }
}

bool ImportVisualStudioDetailsPage::validatePage() 
{
  QMap<QString, STI::Settings::FileTypes::Type>::const_iterator e = mParentWizard->mFileTypes.constBegin();
  while (e != mParentWizard->mFileTypes.constEnd()) {
    if (e.value() == STI::Settings::FileTypes::Cpp) {
      QString extensionString = e.key();
      extensionString.replace(".","");
      if (mParentWizard->mImportSearchContents.contains(extensionString.toLower()) || mParentWizard->mImportSearchContents.contains(extensionString.toUpper())) {
        mParentWizard->mHasImportCPPFiles = true;
        break;
      }
    }
    ++e;
  }
  return true;
}

int ImportVisualStudioDetailsPage::nextId() const
{
  return NewWizard::Page_Conclusion;
}

ImportJsonDetailsPage::ImportJsonDetailsPage(NewWizard * wizardParent)
  : QWizardPage(wizardParent), mParentWizard(wizardParent)
{
  setTitle(tr("Verify CMake Import Settings"));
  setSubTitle(tr("Verify that the file list from the CMake project looks correct."));

  mTopLabel = new QLabel(this);
  mCMakeListWidget = new QListWidget(this);

  bottomLabel = new QLabel(tr("<a href='www.google.com'>More info</a>"));
  QString bottomText = tr("Normally the Understand project stays synchronized with the CMake project, "
                          "so if a file is added to the CMake project it will automatically be added to the Understand project. "
                          "Check this option if you do not want them synchronized. We will still create your initial "
                          "Understand project with the same settings as the CMake project.");
  QString bottomTextWithHeader = tr("<i><Strong>One Time Import</Strong></i><br><br>%1").arg(bottomText);
  bottomLabel->setToolTip(bottomText);
  connect(bottomLabel, &QLabel::linkActivated, [this, bottomTextWithHeader]
  {
    QMessageBox::information(this, tr("New Project Wizard Help"), bottomTextWithHeader);
  });
  mFormLayout = new QFormLayout(this);
  mFormLayout->addRow(tr("Source File:"), mTopLabel);
  mFormLayout->addRow(mCMakeListWidget);
  
  QHBoxLayout * bottomLayout = new QHBoxLayout(); 
  bottomLayout->addWidget(mParentWizard->mCMakeOneTimeImport);
  bottomLayout->addWidget(bottomLabel);
  mFormLayout->addRow(bottomLayout);

  setLayout(mFormLayout);
}

void ImportJsonDetailsPage::initializePage()
{
  mCMakeListWidget->clear();
  mParentWizard->mPathTaken = NewWizard::Path_ImportCMake;
  
  QString newString = mParentWizard->mImportName;
  QString newElideString;
  // Elid in the center...
  QFontMetrics fMetrics = fontMetrics();
  newElideString = fMetrics.elidedText(newString, Qt::ElideMiddle, mFormLayout->sizeHint().width());
  mTopLabel->setText(newElideString);
  mTopLabel->setToolTip(newString);
  UpdateCMakeList(newString);
}

void ImportJsonDetailsPage::UpdateCMakeList(
  const QString &text)
{
  mCMakeListWidget->setEnabled(false);
  mParentWizard->mImportSearchContents.clear();
  mParentWizard->mImportList.clear();
  mParentWizard->mHasImportCPPFiles = false;
  // cleanup filename
  QString filename = QDir::toNativeSeparators(QFileInfo(text).absoluteFilePath());

  if (QFileInfo(filename).isAbsolute()) {

    CMake::CompileDatabase cd(filename);
    
    if (cd.isValid()) {
      foreach (const QString &file, cd.files()) {
        QFileInfo fileInfo(file);
        QString fileExtention = fileInfo.suffix();
        int extentionValue = 0;

        //if (mParentWizard->mFileExtensions.contains(fileExtention)) {
          if (mParentWizard->mImportSearchContents.contains(fileExtention)) {
            extentionValue = mParentWizard->mImportSearchContents.value(fileExtention);
          }
          extentionValue++;
          mParentWizard->mImportSearchContents.insert(fileExtention, extentionValue);
          mCMakeListWidget->addItem(file);
          mParentWizard->mImportList << file;
        }
      //}
    }

  }
  
  mCMakeListWidget->setEnabled(mCMakeListWidget->count());
}

bool ImportJsonDetailsPage::validatePage() 
{
  QMap<QString, STI::Settings::FileTypes::Type>::const_iterator e = mParentWizard->mFileTypes.constBegin();
  while (e != mParentWizard->mFileTypes.constEnd()) {
    if (e.value() == STI::Settings::FileTypes::Cpp) {
      QString extensionString = e.key();
      extensionString.replace(".","");
      if (mParentWizard->mImportSearchContents.contains(extensionString.toLower()) || mParentWizard->mImportSearchContents.contains(extensionString.toUpper())) {
        mParentWizard->mHasImportCPPFiles = true;
        break;
      }
    }
    ++e;
  }
  return true;
}

int ImportJsonDetailsPage::nextId() const
{
  return NewWizard::Page_Conclusion;
}

//New watch page content
WatchBuildPage::WatchBuildPage(NewWizard * wizardParent)
 : QWizardPage(wizardParent), mParentWizard(wizardParent)
 {
  setTitle(tr("Watch your build?"));
  topLabel = new QLabel(tr("Watch your build as it progresses. <a href='www.scitools.com'>More info</a>"));
  //topLabel = new QLabel(this);
  topLabel->setWordWrap(true);
  connect(topLabel, &QLabel::linkActivated, [this]()
  {
    mParentWizard->showHelp();
  });
  //mWatchButton = new QPushButton("Watch",this);
  mWatchButton = new QPushButton("Watch",this);
  bs = new BuildStalker(this);
  loaderLabel = new QLabel;
  loader = new QMovie("images/loader.gif");
  bool watching = false;

  connect(mWatchButton, &QToolButton::clicked, [this, &watching](){
    if (watching){
      mWatchButton->setText("Stop Watching");
      loaderLabel->setMovie(loader);
      loaderLabel->show();
      loader->start();
      bs->start();
  } else {
      mWatchButton->setText("Watch");
      loaderLabel->clear();
      bs->stop();
  }
  watching = !watching;
  });
  
  //mListWidget = new QListWidget(this);
  mListWidget = new QListWidget(this);
  mListWidget->setResizeMode(QListView::Adjust);
  mListWidget->setSortingEnabled(false);

  connect(bs, &BuildStalker::commandAdded,[this](const QString &command){
    mListWidget->addItem(command);
  });
 }

int WatchBuildPage::nextId() const
{
 return NewWizard::Page_StrictDetails;
}


void WatchBuildPage::initializePage()
{
  mParentWizard->mPathTaken = NewWizard::Path_Standard;
  mParentWizard->button(QWizard::NextButton)->setText(tr("Continue"));
}


StrictDetailsPage::StrictDetailsPage(NewWizard * wizardParent)
  : QWizardPage(wizardParent), mParentWizard(wizardParent)
{
  setTitle(tr("Use the Strict or Fuzzy C++ Analysis?"));
  setSubTitle(tr("We have two ways we can analyze C++ code, which would you like to use?"));

  QLabel * FuzzyLabel = new QLabel(tr("The Fuzzy approach is <i><Strong>faster and more forgiving, but less accurate</Strong></i>. "
                                       "If you think the source code might be incomplete or might not compile, "
                                       "or if you just want a quick overview of your code this might be your best option. "
                                       "It doesn't handle overloaded or templated functions well, and newer language variants are not supported."));
  QLabel * strictLabel = new QLabel(tr("Strict may require <i><Strong>more setup, but can provide a much more accurate project</Strong></i>, including support for templates and overloads "
                                      "as well as newer language variants like Objective C and C++ 14. Include paths and macros need to be defined "
                                      "correctly with this option or Understand will return invalid or incomplete results. "));

  strictLabel->setWordWrap(true);
  FuzzyLabel->setWordWrap(true);

  QHBoxLayout * labelLayout = new QHBoxLayout();
  labelLayout->addSpacing(20);
  labelLayout->addWidget(FuzzyLabel);
  
  QHBoxLayout * labelLayout2 = new QHBoxLayout();
  labelLayout2->addSpacing(20);
  labelLayout2->addWidget(strictLabel);
  
  QFormLayout *layout = new QFormLayout(this);
  layout->addRow(mParentWizard->mStrictRadioButton);
  layout->addRow(labelLayout2);

  layout->addRow(mParentWizard->mFuzzyRadioButton);
  layout->addRow(labelLayout);
  
  setLayout(layout);
}

int StrictDetailsPage::nextId() const
{
  return NewWizard::Page_Conclusion;
}

void StrictDetailsPage::initializePage()
{
}

ConclusionPage::ConclusionPage(NewWizard * wizardParent)
  : QWizardPage(wizardParent), mParentWizard(wizardParent)
{
  setTitle(tr("Create your Understand project"));
  setSubTitle(tr("Project Summary:"));

  ////setPixmap(QWizard::WatermarkPixmap, QPixmap(":/images/watermark.png"));
  //setPixmap(QWizard::BackgroundPixmap, QPixmap(":/images/understandWizardcolor.png"));
  
  mParentWizard->mLanguageOptionsListWidget->clear();
  mParentWizard->mLanguageOptionsListWidget->setSortingEnabled(true);

  conclusionAddFilenameButton = new QToolButton(this);
  conclusionAddFilenameButton->setText(tr("..."));
  QLabel * textLabel = new QLabel(tr("Where would you like to save your project?"));
  
  connect(conclusionAddFilenameButton, &QToolButton::clicked, [this]() {
    QString createDir = mParentWizard->mProjectNameLineEdit->text();
    
    if (createDir.isEmpty() && mParentWizard->mSourceCodeListWidget->count() > 0) {
      createDir = mParentWizard->mSourceCodeListWidget->item(0)->text();
    }
    
    // Otherwise, prompt to create a new project.
    QString name = QFileDialog::getSaveFileName(
                     nullptr, tr("Create new project as..."), createDir,
                     tr("%1 projects (*.udb)").arg(QApplication::applicationName()));
    if (!name.isEmpty()) {
      mParentWizard->mProjectNameLineEdit->setText(name);
    }
  });
  
  QHBoxLayout * saveAsLayout = new QHBoxLayout;
  saveAsLayout->addWidget(mParentWizard->mProjectNameLineEdit);
  saveAsLayout->addWidget(conclusionAddFilenameButton);
  //saveAsLayout->addItem(new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum));
  
  QFormLayout *layout = new QFormLayout(this);
  layout->addRow(mParentWizard->mLanguageOptionsListWidget);
  layout->addRow(textLabel);
  //layout->addRow(conclusionAddFilenameButton, mParentWizard->mProjectNameLineEdit);
  //layout->addRow(mParentWizard->mProjectNameLineEdit, conclusionAddFilenameButton);
  layout->addRow(saveAsLayout);
  layout->addRow(mParentWizard->mOpenAdvancedSettingsCheckbox);
  setLayout(layout);
  
  registerField("conclusion.done*", mParentWizard->mProjectNameLineEdit);
}

int ConclusionPage::nextId() const
{
  return -1;
}

void ConclusionPage::initializePage()
{
  mParentWizard->mLanguageOptionsListWidget->clear();

  //set to a default name
  QString basedir;
  QString basename = tr("MyUnderstandProject");
  if (mParentWizard->mSourceCodeListWidget->count() > 0) {
    basedir = mParentWizard->mSourceCodeListWidget->item(0)->text();
    QFileInfo fileDir = QFileInfo(basedir);
    if (fileDir.exists() && !fileDir.fileName().isEmpty() && fileDir.fileName() != QDir::separator()) {
      basename = fileDir.fileName();
    }
  } else {
    basedir = QDir::toNativeSeparators(QDir::homePath());
  }

  basedir = QDir::toNativeSeparators(basedir);
  QString name;
  int i = 0;
  do {
    QString num = i ? QString::number(i) : QString();
    if (basedir.endsWith(QDir::separator())) {
      name = QString("%1%2%3.udb").arg(basedir, basename, num);
    } else {
      name = QString("%1/%2%3.udb").arg(basedir, basename, num);
    }
    name = QDir::toNativeSeparators(name);
    ++i;
  } while (QFileInfo(name).exists());

  mParentWizard->mProjectNameLineEdit->setText(name);
  
  //QMap<Settings::Languages::Language, int> searchList;
  mParentWizard->mSearchList.clear();
  QString searchKey;

  if (mParentWizard->mPathTaken == NewWizard::Path_Standard) {
    mParentWizard->mOpenAdvancedSettingsCheckbox->setVisible(true);
    QMap<QString, int>::const_iterator i = mParentWizard->mSearchContents.constBegin();
    while (i != mParentWizard->mSearchContents.constEnd()) {
      searchKey = i.key();
      if (!searchKey.startsWith(".")) {
        searchKey.prepend(".");
      }
      STI::Settings::FileTypes::Type type = STI::Settings::FileTypes().GetType(searchKey);
      Settings::Languages::Language lang = Settings::FileTypes::Language(type);
      QString searchType = "Other";
      if (Settings::Languages::Valid(lang)) {
        searchType = Settings::Languages::Text(lang);
      }
      
      int searchValue = 0;
      if (mParentWizard->mSearchList.contains(lang)) {
        searchValue = mParentWizard->mSearchList.value(lang);
      }
      
      searchValue = searchValue + i.value();
      mParentWizard->mSearchList.insert(lang, searchValue);
      ++i;
    }
  } else {
    
    if (mParentWizard->mPathTaken == NewWizard::Path_ImportCMake) {
      QString newString = mParentWizard->mImportName;
      mParentWizard->mImportSearchContents.clear();
      mParentWizard->mImportList.clear();
      mParentWizard->mHasImportCPPFiles = false;
      // cleanup filename
      QString filename = QDir::toNativeSeparators(QFileInfo(newString).absoluteFilePath());

      if (QFileInfo(filename).isAbsolute()) {

        CMake::CompileDatabase cd(filename);
        
        if (cd.isValid()) {
          foreach (const QString &file, cd.files()) {
            QFileInfo fileInfo(file);
            QString fileExtention = fileInfo.suffix();
            int extentionValue = 0;

            if (mParentWizard->mImportSearchContents.contains(fileExtention)) {
              extentionValue = mParentWizard->mImportSearchContents.value(fileExtention);
            }
            extentionValue++;
            mParentWizard->mImportSearchContents.insert(fileExtention, extentionValue);
            mParentWizard->mImportList << file;
          }
        }
      }
    }
    
    mParentWizard->mOpenAdvancedSettingsCheckbox->setVisible(true);
    QMap<QString, int>::const_iterator i = mParentWizard->mImportSearchContents.constBegin();
    while (i != mParentWizard->mImportSearchContents.constEnd()) {
      searchKey = i.key();
      if (!searchKey.startsWith(".")) {
        searchKey.prepend(".");
      }
      STI::Settings::FileTypes::Type type = STI::Settings::FileTypes().GetType(searchKey);
      Settings::Languages::Language lang = Settings::FileTypes::Language(type);
      QString searchType = "Other";
      if (Settings::Languages::Valid(lang)) {
        searchType = Settings::Languages::Text(lang);
      }
      
      int searchValue = 0;
      if (mParentWizard->mSearchList.contains(lang)) {
        searchValue = mParentWizard->mSearchList.value(lang);
      }
      
      searchValue = searchValue + i.value();
      mParentWizard->mSearchList.insert(lang, searchValue);
      ++i;
    }
    
  }
  
  mParentWizard->mLanguageOptionsListWidget->setSelectionMode(QAbstractItemView::NoSelection);
  mParentWizard->mLanguageOptionsListWidget->setFocusPolicy(Qt::NoFocus);
  QMap<Settings::Languages::Language, int>::const_iterator s = mParentWizard->mSearchList.constBegin();
  while (s != mParentWizard->mSearchList.constEnd()) {
    
    QString tempCount;
    QString searchType = "Other";
    if (Settings::Languages::Valid(s.key())) {
      searchType = Settings::Languages::Text(s.key());
    }
    
    double sValue = s.value();
    tempCount = QString("%1: %2 Files").arg(searchType).arg(QLocale::system().toString(sValue, 'f', 0));
        
    QString langName = searchType;
    langName.replace("#", "Sharp");
    langName.replace("Assembly", "Assembler");


    QListWidgetItem * sourceItem = new QListWidgetItem( tempCount, mParentWizard->mLanguageOptionsListWidget);
    sourceItem->setData(Qt::UserRole, s.key());
    mParentWizard->mLanguageOptionsListWidget->addItem(sourceItem);

    QWidget* newWidget = new QWidget();

    QHBoxLayout * hBoxLayout = new QHBoxLayout();
    hBoxLayout->addItem(new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum));

    QToolButton * hiddenButton = new QToolButton();
    bool hasHiddenButton = false;
    //hiddenButton->hide();

    switch (s.key()) {
      case Settings::Languages::Ada:
          hBoxLayout->addWidget(mParentWizard->mAdaVersionComboBox, 0);
          break;
      case Settings::Languages::Assembly:
        hBoxLayout->addWidget(mParentWizard->mAssemblyAssemblerComboBox, 0);
        break;
      case Settings::Languages::Basic: // has no additional configuration options at this time
        hBoxLayout->addWidget(hiddenButton);
        hasHiddenButton = true; 
        break;
      case Settings::Languages::Cobol: {
        hBoxLayout->addWidget(mParentWizard->mCobolCompilerComboBox, 0);
        hBoxLayout->addWidget(mParentWizard->mCobolFormatComboBox, 0);
        break;
      }
      case Settings::Languages::Cpp: {
        if (mParentWizard->mStrictRadioButton->isChecked()) {
          hBoxLayout->addWidget(mParentWizard->mCppStrictCompilerComboBox);
        } else {
          hBoxLayout->addWidget(mParentWizard->mCppCompilerComboBox);
        }
        break;
      }
      case Settings::Languages::Csharp: // has no additional configuration options at this time
        hBoxLayout->addWidget(hiddenButton);
        hasHiddenButton = true;
        break;
      case Settings::Languages::Fortran:
        hBoxLayout->addWidget(mParentWizard->mFortranVersionComboBox, 0);
        hBoxLayout->addWidget(mParentWizard->mFortranFormatComboBox, 0);
        break;
      case Settings::Languages::Java:
        hBoxLayout->addWidget(mParentWizard->mJavaVersionComboBox, 0);
        break;
      case Settings::Languages::Jovial:
        hBoxLayout->addWidget(mParentWizard->mJovialVersionComboBox, 0);
        break;
      case Settings::Languages::Pascal:
        hBoxLayout->addWidget(mParentWizard->mPascalVersionComboBox, 0);
        break;
      case Settings::Languages::Plm:
        hBoxLayout->addWidget(mParentWizard->mPlmVersionComboBox, 0);
        break;
      case Settings::Languages::Python:
        hBoxLayout->addWidget(mParentWizard->mPythonVersionComboBox, 0);
        break;
      case Settings::Languages::Vhdl:
        hBoxLayout->addWidget(hiddenButton);
        hasHiddenButton = true;
        break;
      case Settings::Languages::Web:
        hBoxLayout->addWidget(mParentWizard->mWebVersionComboBox, 0);
        break;
      default:
        hBoxLayout->addWidget(hiddenButton);
        hasHiddenButton = true;
        break;
    }
    
    hBoxLayout->addSpacing(4);
    hBoxLayout->setMargin(1);

    newWidget->setLayout(hBoxLayout);
    //newWidget->setContentsMargins(0,10,0,10);

    sourceItem->setSizeHint(newWidget->sizeHint());
    if (hasHiddenButton) {
      hiddenButton->hide();
      newWidget = new QWidget();
      delete hiddenButton;
    }
    mParentWizard->mLanguageOptionsListWidget->setItemWidget(sourceItem, newWidget);
    ++s;
  }
  mParentWizard->mInitializeOptions = true; //reinitialize options if the user goes back to the import page
}

//******************************************
// NewWizardMessage Functions
//******************************************
NewWizardMessage::NewWizardMessage(NewWizard * wizardParent)
  : Settings::Files::Msg(), mParentWizard(wizardParent)
{
}


void NewWizardMessage::AddFilesFound(
  int foundFiles)
{
  mParentWizard->mNumberOfToBeAddedFiles = foundFiles;
  mParentWizard->changeProgressDialog();
}

void NewWizardMessage::AddFile(
  Settings::AutoFile *autofile)
{
  mNumAdded++;
  
  ++mParentWizard->mNumberOfAddedFiles;
  
  if (mParentWizard->mNumberOfAddedFiles % 7 == 0)
  {
    mParentWizard->updateProgressDialog();
    TestAbort();
  }
}


void NewWizardMessage::TestAbort()
{
  QCoreApplication::instance()->processEvents();
  if (mParentWizard->mBreakout) {
    Abort();
    Settings::Files::Msg::Abort();
  }
}


bool NewWizardMessage::CheckDirectory(
  const QString &name, const bool topLevel)
{
  
  ++mParentWizard->mNumberOfAddedDirectories;
  
  if (mParentWizard->mNumberOfAddedDirectories % 17 == 0)
  {
    mParentWizard->updateProgressDialog();
    TestAbort();
  }
  
  return true;
}


void NewWizardMessage::DeleteFile(
  const QString &name)
{
  mNumRemoved++;
}


void NewWizardMessage::MatchFile(
  const QString &name)
{
  mNumAdded++;
  
  ++mParentWizard->mNumberOfAddedFiles;
  
  if (mParentWizard->mNumberOfAddedFiles % 7 == 0)
  {
    mParentWizard->updateProgressDialog();
    TestAbort();
  }
}


QStringList &NewWizardMessage::getMatchedFiles()
{
  return mMatchedFiles;
}


int NewWizardMessage::getNumAdded()
{
  return mNumAdded;
}


int NewWizardMessage::getNumRemoved()
{
  return mNumRemoved;
}

}
}
