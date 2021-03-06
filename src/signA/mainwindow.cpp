#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <functional>

#include <QElapsedTimer>
//#include "TxtQuickImportWizDlg.h"
//----------Qt---------------
#include <QMessageBox>
#include <QPluginLoader>
#include <QFileDialog>
#include <QColorDialog>
#include <QDate>
#include <QLocale>
#include <QTextCodec>
#include <QItemSelectionModel>
#include <QElapsedTimer>
#include <QSettings>
#include <QInputDialog>
#include <QInputDialog>
#include <QMdiArea>
#include <QProcess>
//----------STL-------------
#include <iostream>
#include <algorithm>
#include <limits>
#include <functional>
#include <memory>

//----------SA--------------

// |------Dialog------------
#include <Dialog_AddChart.h>
#include "CurveSelectDialog.h"
#include "SAProjectInfomationSetDialog.h"


#include <PickCurveDataModeSetDialog.h>
#include <SAPropertySetDialog.h>

// |------widget
#include <SATabValueViewerWidget.h>
#include <SAMessageWidget.h>
#include "SAFigureWindow.h"
// |------操作

//===signACommonUI
#include "SAUI.h"
#include "SAUIReflection.h"//ui管理器
#include "SAValueSelectDialog.h"
//===signALib
#include "SALocalServerDefine.h"
#include "SAValueManager.h"//变量管理
#include "SAValueManagerModel.h"//变量管理的model
#include "SAPluginInterface.h"
#include "SALog.h"
#include "SAProjectManager.h"
#include "SAValueManagerMimeData.h"
//===SAChart
#include "SAChart.h"


#include <SAThemeManager.h>

// |------代理
//#include <SAFuctionDelegate.h>

// |-----chart
#include <SAPlotMarker.h>
#include "SAChart3D.h"
// |-----common ui
#include "SAChart2D.h"
// |-----宏-----------------
#include "DebugInfo.h"
// |-----czy
#include <czyQtUI.h>
#include <czyQtApp.h>
#include <czyMath_Interpolation.h>
// |-----model class --------------
#include <MdiWindowModel.h>
#include <QwtPlotItemDataModel.h>
#include <QwtPlotItemTreeModel.h>
#include "DataFeatureTreeModel.h"
#include <SAVariantHashTableModel.h>
#include <SACsvFileTableModel.h>
#include <SAPlotLayerModel.h>
//--------3thparty-----------


//
//--使用策略模式构建sa里的所有方法
//--改Global里，用SA的枚举替换所有的宏

//#include <SAFigure.h>




//#include <qwt3d_gridplot.h>
//#include <layerItemDelegate.h>


//--删除表格数据发生错误
static QColor static_line_color[9] = {
    Qt::black
    ,Qt::red
    ,Qt::green
    ,Qt::blue
    ,Qt::cyan
    ,Qt::magenta
    ,Qt::yellow
    ,Qt::gray
    ,Qt::lightGray
};

QColor MainWindow::getRandColor()
{
    return static_line_color[int (float(qrand())/float(RAND_MAX)*9.0)];
}

void debug();









MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
  ,uiInterface(new SAUI(this))
  ,ui_status_progress(nullptr)
  ,ui_status_info(nullptr)
  ,m_menuTreeProjectItem(nullptr)
  ,m_menuValueManagerView(nullptr)
  ,m_lastActiveWnd(nullptr)
  ,m_nProjectCount(0)
  ,m_nUserChartCount(0)
  ,m_lastShowFigureWindow(nullptr)
{
    ui->setupUi(this);
    saAddLog("start app");
    saStartElapsed("start main app init");
    QWidget* p = takeCentralWidget();
    if(p)
        delete p;//移除中央窗口
    init();
    initUI();
    initMenu();
    initUIReflection();
    initProcess();
    saElapsed("init ui and menu");
    saStartElapsed("start load plugin and theme");
    ui->toolBar_chartSet->setEnabled(false);
    initPlugin();
    initTheme();
    saElapsed("loaded plugins and themes");
    showNormalMessageInfo(QStringLiteral("程序初始化完成"));
}


void debug(){

}

///
/// \brief 程序初始化
///
void MainWindow::init()
{
	//状态栏的进度条

    ui_status_info = new SAInformationStatusWidget(this);
    ui->statusBar->addWidget(ui_status_info);
    ui_status_info->setVisible (false);
    ui_status_info->setFadeIn (true,500,5);
    ui_status_info->setFadeOut (true,500,5);
    ui_status_progress = new progressStateWidget(this);
    ui->statusBar->addWidget(ui_status_progress);
    ui_status_progress->setVisible(false);
}

#define Lambda_SaChartEnable(exp)\
    [&](bool check){\
    SAChart2D* chart = getCurSubWindowChart();\
    if(chart)\
{\
    if(check)\
{\
    if(!chart->isEnable##exp())\
    chart->enable##exp(check);\
    }\
    else\
{\
    if(chart->isEnable##exp())\
    chart->enable##exp(check);\
    }\
    }\
    }\
    ///
/// \brief 界面初始化
///
void MainWindow::initUI()
{

    loadSetting();
    setDockNestingEnabled(true);
    setDockOptions(QMainWindow::AnimatedDocks|QMainWindow::AllowTabbedDocks|QMainWindow::AllowNestedDocks);
    //mdi窗口管理器关联
    m_mdiManager.setMdi(ui->mdiArea);
    //项目结构树
    m_drawDelegate.reset (new SADrawDelegate(this));

	//////////////////////////////////////////////////////////////////////////
	//model
	//////////////////////////////////////////////////////////////////////////
    //子窗口列表的model
    MdiWindowModel* mdiListModel = new MdiWindowModel(ui->mdiArea,ui->listView_window);
    ui->listView_window->setModel(mdiListModel);
    connect(ui->listView_window,&QAbstractItemView::clicked,mdiListModel,&MdiWindowModel::onItemClick);
    connect(ui->listView_window,&QAbstractItemView::doubleClicked,mdiListModel,&MdiWindowModel::onItemDoubleClick);
    //-------------------------------------
    // - 图层的设置
	SAPlotLayerModel* layerModel = new SAPlotLayerModel(ui->tableView_layer);
	ui->tableView_layer->setModel(layerModel);
	auto hh = ui->tableView_layer->horizontalHeader();
	hh->setSectionResizeMode(0,QHeaderView::ResizeToContents);
	hh->setSectionResizeMode(1,QHeaderView::ResizeToContents);
	hh->setSectionResizeMode(2,QHeaderView::ResizeToContents);
	hh->setStretchLastSection(true);
    connect(ui->tableView_layer,&QTableView::pressed,this,&MainWindow::onTableViewLayerPressed);
    //------------------------------------------------------------
    //- dataviewer dock 数据观察dock的相关设置
    QwtPlotItemDataModel* qwtDataModel = new QwtPlotItemDataModel(this);
    ui->tableView_curSelItemDatas->setModel(qwtDataModel);
    QwtPlotItemTreeModel* qwtItemTreeModel = new QwtPlotItemTreeModel(this);
    ui->treeView_curPlotItem->setModel(qwtItemTreeModel);
    qwtItemTreeModel->setFilter (QList<QwtPlotItem::RttiValues>()
                          <<QwtPlotItem::Rtti_PlotCurve
                          <<QwtPlotItem::Rtti_PlotBarChart
                          <<QwtPlotItem::Rtti_PlotHistogram
                          <<QwtPlotItem::Rtti_PlotIntervalCurve
                          <<QwtPlotItem::Rtti_PlotTradingCurve
                          );
    ui->splitter_chartDataViewer->setStretchFactor(0,1);
    ui->splitter_chartDataViewer->setStretchFactor(1,3);
    //-------------------------------------
    // - start valueManager signal/slots connect
    connect(ui->treeView_valueManager,&QTreeView::clicked,this,&MainWindow::onTreeViewValueManagerClicked);
    connect(ui->treeView_valueManager,&QTreeView::doubleClicked,this,&MainWindow::onTreeViewValueManagerDoubleClicked);
    connect(ui->treeView_valueManager,&QTreeView::customContextMenuRequested,this,&MainWindow::onTreeViewValueManagerCustomContextMenuRequested);
    connect(ui->actionRenameValue,&QAction::triggered,this,&MainWindow::onActionRenameValueTriggered);
    //-------------------------------------
    // - start subwindow signal/slots connect
    connect(ui->mdiArea,&QMdiArea::subWindowActivated,this,&MainWindow::onMdiAreaSubWindowActivated);
    //-------------------------------------
    // - start file menu signal/slots connect
    connect(ui->actionOpen,&QAction::triggered,this,&MainWindow::onActionOpenTriggered);
    connect(ui->actionOpenProject,&QAction::triggered,this,&MainWindow::onActionOpenProjectTriggered);
    connect(ui->actionSave,&QAction::triggered,this,&MainWindow::onActionSaveTriggered);
    connect(ui->actionSaveAs,&QAction::triggered,this,&MainWindow::onActionSaveAsTriggered);
    connect(ui->actionClearProject,&QAction::triggered,this,&MainWindow::onActionClearProjectTriggered);
    //-------------------------------------
    // - start chart set menu signal/slots connect
    connect(ui->actionNewChart,&QAction::triggered,this,&MainWindow::onActionNewChartTriggered);
    connect(ui->actionNewTrend,&QAction::triggered,this,&MainWindow::onActionNewTrendTriggered);
    //-------------------------------------
    // - menu_chartDataManager menu signal/slots connect
    connect(ui->actionInRangDataRemove,&QAction::triggered,this,&MainWindow::onActionInRangDataRemoveTriggered);
    connect(ui->actionPickCurveToData,&QAction::triggered,this,&MainWindow::onActionPickCurveToDataTriggered);
    //-------------------------------------
    // - menu_dataManager menu signal/slots connect
    connect(ui->actionViewValueInCurrentTab,&QAction::triggered,this,&MainWindow::onActionViewValueInCurrentTabTriggered);
    connect(ui->actionViewValueInNewTab,&QAction::triggered,this,&MainWindow::onActionViewValueInNewTabTriggered);
    connect(ui->actionDeleteValue,&QAction::triggered,this,&MainWindow::onActionDeleteValueTriggered);
    //-------------------------------------
    // - about menu signal/slots connect
    connect(ui->actionAbout,&QAction::triggered,this,&MainWindow::onActionAboutTriggered);
    //-------------------------------------
    // - TreeView CurPlotItem slots(曲线条目树形窗口)
    connect(ui->treeView_curPlotItem,&QTreeView::clicked,this,&MainWindow::onTreeViewCurPlotItemClicked);
    //-------------------------------------
    // - TreeView CurPlotItem slots(曲线条目树形窗口)
    connect(ui->actionRescind,&QAction::triggered,this,&MainWindow::onActionRescindTriggered);
    connect(ui->actionRedo,&QAction::triggered,this,&MainWindow::onActionRedoTriggered);
    //-------------------------------------
    // - tool menu signal/slots connect
    connect(ui->actionProjectSetting,&QAction::triggered,this,&MainWindow::onActionProjectSettingTriggered);
    //------------------------------------------------------------
    //- window menu 窗口 菜单
    connect(ui->actionSetDefalutDockPos,&QAction::triggered,this,&MainWindow::onActionSetDefalutDockPosTriggered);
    //窗口模式
    connect(ui->actionWindowMode,&QAction::triggered,[&](){
        czy::QtApp::QWaitCursor waitCur;
        Q_UNUSED(waitCur);
        ui->actionTabMode->setChecked(false);
        ui->actionWindowMode->setChecked(true);
        if(QMdiArea::SubWindowView == ui->mdiArea->viewMode()){
            return;
        }
        ui->mdiArea->setViewMode(QMdiArea::SubWindowView);
    });
    //标签模式
    connect(ui->actionTabMode,&QAction::triggered,[&](){
        czy::QtApp::QWaitCursor waitCur;
        Q_UNUSED(waitCur);
        ui->actionTabMode->setChecked(true);
        ui->actionWindowMode->setChecked(false);

        if(QMdiArea::TabbedView == ui->mdiArea->viewMode()){
            return;
        }
        ui->mdiArea->setViewMode(QMdiArea::TabbedView);
    });
    //层叠布置
    connect(ui->actionWindowCascade,&QAction::triggered,[&](){
        czy::QtApp::QWaitCursor waitCur;
        Q_UNUSED(waitCur);
        if(QMdiArea::SubWindowView == ui->mdiArea->viewMode()){
            ui->mdiArea->cascadeSubWindows();
        }
    });
    //均匀布置
    connect(ui->actionWindowTile,&QAction::triggered,[&](){
        czy::QtApp::QWaitCursor waitCur;
        Q_UNUSED(waitCur);
        if(QMdiArea::SubWindowView == ui->mdiArea->viewMode()){
            ui->mdiArea->tileSubWindows();
        }
    });

    //显示隐藏dock窗口
    connect(ui->actionDataFeatureDock,&QAction::triggered,[&](){ui->dockWidget_DataFeature->show();});
    connect(ui->actionSubWindowListDock,&QAction::triggered,[&](){ui->dockWidget_windowList->show();});
    connect(ui->actionValueManagerDock,&QAction::triggered,[&](){ui->dockWidget_valueManage->show();});
    connect(ui->actionLayerOutDock,&QAction::triggered,[&](){ui->dockWidget_plotLayer->show();});
    connect(ui->actionValueViewerDock,&QAction::triggered,[&](){ui->dockWidget_valueViewer->show();});
    connect(ui->actionFigureViewer,&QAction::triggered,[&](){ui->dockWidget_main->show();});
    //===========================================================
    //- 图表设置菜单及工具栏的关联
    //十字光标
    ui->actionEnableChartPicker->setCheckable(true);
    connect(ui->actionEnableChartPicker,&QAction::triggered
            ,this,&MainWindow::onActionEnableChartPicker);

    //拖动
    ui->actionEnableChartPanner->setCheckable(true);
    connect(ui->actionEnableChartPanner,&QAction::triggered
            ,this,&MainWindow::onActionEnableChartPanner);
    //区间缩放
    ui->actionEnableChartZoom->setCheckable(true);
    connect(ui->actionEnableChartZoom,&QAction::triggered
            ,this,&MainWindow::onActionEnableChartZoom);


    QToolButton* toolbtn = qobject_cast<QToolButton*>(ui->toolBar_chartSet->widgetForAction(ui->actionEnableChartZoom));
    if(toolbtn)
    {
        QMenu* m1 = new QMenu(toolbtn);
        m1->addAction(ui->actionZoomIn);
        m1->addAction(ui->actionZoomOut);
        m1->addAction(ui->actionZoomBase);
        m1->addAction(ui->actionChartZoomReset);
        toolbtn->setPopupMode(QToolButton::MenuButtonPopup);
        toolbtn->setMenu(m1);
    }
    connect(ui->actionZoomBase,&QAction::triggered
            ,this,&MainWindow::onActionChartZoomToBase);
    connect(ui->actionChartZoomReset,&QAction::triggered
            ,this,&MainWindow::onActionChartZoomReset);
    connect(ui->actionZoomIn,&QAction::triggered
            ,this,&MainWindow::onActionChartZoomIn);
    connect(ui->actionZoomOut,&QAction::triggered
            ,this,&MainWindow::onActionChartZoomOut);

    //矩形选框
    ui->actionStartRectSelect->setCheckable(true);
    connect(ui->actionStartRectSelect,&QAction::triggered
            ,this,&MainWindow::onActionStartRectSelectTriggered);
    //椭圆选框
    ui->actionStartEllipseSelect->setCheckable(true);
    connect(ui->actionStartEllipseSelect,&QAction::triggered
            ,this,&MainWindow::onActionStartEllipseSelectTriggered);
    //多边形选框
    ui->actionStartPolygonSelect->setCheckable(true);
    connect(ui->actionStartPolygonSelect,&QAction::triggered
            ,this,&MainWindow::onActionStartPolygonSelectTriggered);
    //清除所有选区
    connect(ui->actionClearAllSelectiedRegion,&QAction::triggered
            ,this,&MainWindow::onActionClearAllSelectiedRegion);

    //数据显示
    ui->actionYDataPicker->setCheckable(true);
    connect(ui->actionYDataPicker,&QAction::triggered,Lambda_SaChartEnable(YDataPicker));
    ui->actionXYDataPicker->setCheckable(true);
    connect(ui->actionXYDataPicker,&QAction::triggered,Lambda_SaChartEnable(XYDataPicker));

    toolbtn = qobject_cast<QToolButton*>(ui->toolBar_chartSet->widgetForAction(ui->actionXYDataPicker));
    if(toolbtn)
    {
        QMenu* m = new QMenu(toolbtn);
        m->addAction(ui->actionYDataPicker);
        toolbtn->setPopupMode(QToolButton::MenuButtonPopup);
        toolbtn->setMenu(m);
    }

    //网格
    ui->actionShowGrid->setCheckable(true);
    connect(ui->actionShowGrid,&QAction::triggered,Lambda_SaChartEnable(Grid));
    toolbtn = qobject_cast<QToolButton*>(ui->toolBar_chartSet->widgetForAction(ui->actionShowGrid));
    if(toolbtn)
    {
        QMenu* m1 = new QMenu(toolbtn);
        m1->addAction(ui->actionShowHGrid);
        m1->addAction(ui->actionShowCrowdedHGrid);
        m1->addAction(ui->actionShowVGrid);
        m1->addAction(ui->actionShowCrowdedVGrid);
        toolbtn->setPopupMode(QToolButton::MenuButtonPopup);
        toolbtn->setMenu(m1);
    }

    //显示水平网格
    ui->actionShowHGrid->setCheckable(true);
    connect(ui->actionShowHGrid,&QAction::triggered,Lambda_SaChartEnable(GridY));
    //显示密集水平网格
    ui->actionShowCrowdedHGrid->setCheckable(true);
    connect(ui->actionShowCrowdedHGrid,&QAction::triggered,Lambda_SaChartEnable(GridYMin));
    //显示垂直网格
    ui->actionShowVGrid->setCheckable(true);
    connect(ui->actionShowVGrid,&QAction::triggered,Lambda_SaChartEnable(GridX));
    //显示密集水平网格
    ui->actionShowCrowdedVGrid->setCheckable(true);
    connect(ui->actionShowCrowdedVGrid,&QAction::triggered,Lambda_SaChartEnable(GridXMin));
    //显示图例
    ui->actionShowLegend->setCheckable(true);
    connect(ui->actionShowLegend,&QAction::triggered,Lambda_SaChartEnable(Legend));
    //显示图例选择器
    ui->actionLegendPanel->setCheckable(true);
    connect(ui->actionLegendPanel,&QAction::triggered,Lambda_SaChartEnable(LegendPanel));


    //
    connect(ui->mdiArea,&QMdiArea::subWindowActivated
            ,ui->dataFeatureWidget,&SADataFeatureWidget::mdiSubWindowActived);
    connect(ui->dataFeatureWidget,&SADataFeatureWidget::showMessageInfo
            ,this,&MainWindow::showMessageInfo);
    //窗口关闭的消息在 on_subWindow_close里


    //saValueManager和saUI的关联
    connect(saValueManager,&SAValueManager::messageInformation
            ,this,&MainWindow::showMessageInfo);
    connect(saValueManager,&SAValueManager::dataRemoved
            ,this,&MainWindow::onDataRemoved);
    //SAProjectManager和saUI的关联
    connect(saProjectManager,&SAProjectManager::messageInformation
            ,this,&MainWindow::showMessageInfo);

    showMaximized();
}



void MainWindow::initMenu()
{
    //变量管理菜单的初始化
    m_menuValueManagerView = new QMenu(ui->treeView_valueManager);
    //在当前标签中打开
    m_menuValueManagerView->addAction(ui->actionViewValueInCurrentTab);
    //在新建标签中打开
    m_menuValueManagerView->addAction(ui->actionViewValueInNewTab);
    m_menuValueManagerView->addSeparator();
    m_menuValueManagerView->addAction(ui->actionRenameValue);//重命名变量
    m_menuValueManagerView->addAction(ui->actionDeleteValue);//删除变量
}

void MainWindow::initPlugin()
{
    m_pluginManager = new SAPluginManager(uiInterface,this);
    connect(m_pluginManager,&SAPluginManager::postInfoMessage
            ,this,&MainWindow::showMessageInfo);

    czy::QtApp::QWaitCursor wait;
    int count = m_pluginManager->loadPlugins();
    QString str = tr("load %1 plugins").arg(count);
    showNormalMessageInfo(str);
}

void MainWindow::initTheme()
{
    //SAThemeManager themeManager(this);
    //themeManager.setTableViewLayout (ui->tableView_layer);
}

void MainWindow::initUIReflection()
{
    saUIRef->setupUIInterface(uiInterface);//saUI保存主窗口指针
}
///
/// \brief 打开用户界面支持的其它进程
///
void MainWindow::initProcess()
{
    QString path = qApp->applicationDirPath()+"/signADataProc.exe";
    QStringList args = {QString::number(qApp->applicationPid())};
    QProcess::startDetached(path,args);//signADataProc是一个单例进程，多个软件不会打开多个
}

///
/// \brief 重置布局
///
void MainWindow::onActionSetDefalutDockPosTriggered()
{
    addDockWidget(Qt::LeftDockWidgetArea,ui->dockWidget_valueManage);//从最左上角的dock开始布置，先把列布置完
    splitDockWidget(ui->dockWidget_valueManage,ui->dockWidget_main,Qt::Horizontal);
    splitDockWidget(ui->dockWidget_main,ui->dockWidget_plotLayer,Qt::Horizontal);
    splitDockWidget(ui->dockWidget_valueManage,ui->dockWidget_windowList,Qt::Vertical);
    splitDockWidget(ui->dockWidget_main,ui->dockWidget_chartDataViewer,Qt::Vertical);
    splitDockWidget(ui->dockWidget_plotLayer,ui->dockWidget_DataFeature,Qt::Vertical);
    tabifyDockWidget(ui->dockWidget_main,ui->dockWidget_valueViewer);
    tabifyDockWidget(ui->dockWidget_chartDataViewer,ui->dockWidget_message);
    tabifyDockWidget(ui->dockWidget_windowList,ui->dockWidget_plotSet);
    ui->dockWidget_valueManage->show();
    //ui->dockWidget_valueManage->resize(QSize(500,ui->dockWidget_valueManage->height()));
    ui->dockWidget_plotSet->show();
    ui->dockWidget_windowList->show();
    ui->dockWidget_plotLayer->show();
    ui->dockWidget_chartDataViewer->show();
    ui->dockWidget_DataFeature->show();
    ui->dockWidget_valueViewer->show();
    ui->dockWidget_main->show();
    ui->dockWidget_message->show();
    ui->dockWidget_chartDataViewer->raise();
    ui->dockWidget_main->raise();

}

void MainWindow::onActionProjectSettingTriggered()
{
    setProjectInfomation();
}




MainWindow::~MainWindow()
{
    saveSetting();
    delete ui;
}

void MainWindow::loadSetting()
{
    QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                       "CZY", "SA");
	loadWindowState(settings);
}

void MainWindow::saveSetting()
{
    QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                       "CZY", "SA");
	//mainWindow
	saveWindowState(settings);
	
}

void MainWindow::saveWindowState(QSettings& setting)
{
    setting.beginGroup("StartTimes");
    setting.setValue("firstStart", false);
    setting.endGroup();

	setting.beginGroup("mainWindow");
	setting.setValue("geometry", saveGeometry());
	setting.setValue("windowState", saveState());
	setting.endGroup();

}
void MainWindow::loadWindowState(const QSettings& setting)
{
	QVariant var = QVariant();
    var = setting.value("StartTimes/firstStart");
    if(var.isValid())
    {
        if(var.toBool())
            onActionSetDefalutDockPosTriggered();
    }
    else
    {
        onActionSetDefalutDockPosTriggered();
    }
	var = setting.value("mainWindow/geometry");
	if(var.isValid())
		restoreGeometry(var.toByteArray());
	var = setting.value("mainWindow/windowState");
	if(var.isValid())
		restoreState(var.toByteArray());

}



///
/// \brief 曲线数据发生变动
/// NOTE: 此函数还未完成，需要注意
/// \param widget
/// \param pC
/// TODO:
void MainWindow::onChartDataChanged(QWidget *widget,const QwtPlotItem *pC)
{
    Q_UNUSED(pC)
    Q_UNUSED(widget)
    //TODO

}


///
/// \brief 变量管理树形视图的单击触发
/// \param index
///
void MainWindow::onTreeViewValueManagerClicked(const QModelIndex &index)
{
    Q_UNUSED(index);
    //把选中的数据传递给saUI
    SAAbstractDatas* data = getSeletedData();
    if(data)
    {
        uiInterface->onSelectDataChanged(data);
        emit selectDataChanged(data);
    }
}
///
/// \brief 变量管理树形视图的双击触发
/// \param index
///
void MainWindow::onTreeViewValueManagerDoubleClicked(const QModelIndex &index)
{
    Q_UNUSED(index);
    QList<SAAbstractDatas*> datas=getSeletedDatas();
    if(datas.size ()<=0)
        return;
    ui->tabWidget_valueViewer->setDataInNewTab(datas);
    if (ui->dockWidget_valueViewer->isHidden())
        ui->dockWidget_valueViewer->show();
    ui->dockWidget_valueViewer->raise();
}
///
/// \brief 变量管理树形视图的右键触发
/// \param pos
///
void MainWindow::onTreeViewValueManagerCustomContextMenuRequested(const QPoint &pos)
{
    Q_UNUSED(pos);
    if(m_menuValueManagerView)
        m_menuValueManagerView->exec (QCursor::pos());
}

SAValueManagerModel*MainWindow::getValueManagerModel() const
{
    return ui->treeView_valueManager->getValueManagerModel();
}


SADrawDelegate*MainWindow::getDrawDelegate() const
{
    return m_drawDelegate.get ();
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    if(event->mimeData()->hasFormat(SAValueManagerMimeData::valueIDMimeType()))
    {
        saPrint() << "dragEnterEvent SAValueManagerMimeData::valueIDMimeType()";
        event->setDropAction(Qt::MoveAction);
        event->accept();
    }
    else
    {
        event->ignore();
    }
}

void MainWindow::dragMoveEvent(QDragMoveEvent *event)
{
    if(event->mimeData()->hasFormat(SAValueManagerMimeData::valueIDMimeType()))
    {
        saPrint() << "dragMoveEvent SAValueManagerMimeData::valueIDMimeType()";
        event->setDropAction(Qt::MoveAction);
        event->accept();
    }
}

void MainWindow::dropEvent(QDropEvent *event)
{
    if(event->mimeData()->hasFormat(SAValueManagerMimeData::valueIDMimeType()))
    {
        QList<int> ids;
        if(SAValueManagerMimeData::getValueIDsFromMimeData(event->mimeData(),ids))
        {
            saPrint() << "dropEvent SAValueManagerMimeData::valueIDMimeType()";
            QList<SAAbstractDatas*> datas = saValueManager->findDatas(ids);
            if(datas.size() > 0)
            {
                m_drawDelegate->drawTrend(datas);
            }
        }
    }
}

///
/// \brief 打开action
///
void MainWindow::onActionOpenTriggered()
{
    QFileDialog openDlg(this);
    QStringList strNFilter = m_pluginManager->getOpenFileNameFilters();

    strNFilter.push_back(tr("all files (*.*)"));
    openDlg.setFileMode(QFileDialog::ExistingFile);
    openDlg.setNameFilters(strNFilter);
    if (QDialog::Accepted == openDlg.exec())
    {
        QStringList strfileNames = openDlg.selectedFiles();
        if(strfileNames.isEmpty())
            return;

        QString strFile = strfileNames.value(0);
        QFileInfo temDir(strFile);
        QString suffix = temDir.suffix();
        suffix = suffix.toLower();
        SAAbstractDataImportPlugin* import = m_pluginManager->getDataImportPluginFromSuffix(suffix);
        if(nullptr != import)
        {
            import->openFile(strfileNames);
        }
    }
}

///
/// \brief 打开项目文件夹
///
void MainWindow::onActionOpenProjectTriggered()
{
    QString path = QFileDialog::getExistingDirectory(this,QStringLiteral("选择项目目录"));
    if(path.isEmpty())
        return;
    saProjectManager->load(path);
}


///
/// \brief 保存
///
void MainWindow::onActionSaveTriggered()
{
    QString projectPath = saProjectManager->getProjectFullPath();
    if(projectPath.isEmpty())
    {
        if(saProjectManager->getProjectName().isEmpty())
        {
            if(!setProjectInfomation())
            {
                showWarningMessageInfo(tr("you need to set a project name"));
                return;
            }
        }
        onActionSaveAsTriggered();
    }
    else
    {
        saProjectManager->save();
    }
}
///
/// \brief 另存为
///
void MainWindow::onActionSaveAsTriggered()
{
    QString path = QFileDialog::getSaveFileName(this,QStringLiteral("保存"),QString()
                                 ,QString(),0,QFileDialog::ShowDirsOnly);
    if(path.isEmpty())
        return;
    saProjectManager->saveAs(path);
}


///
/// \brief 添加新图例
///
void MainWindow::onActionNewChartTriggered()
{
    Dialog_AddChart addChart(this);

    if(QDialog::Accepted == addChart.exec())
    {
        m_nUserChartCount++;
        QString chartName = QStringLiteral("新图例-%1").arg(m_nUserChartCount);
        SAMdiSubWindow* pSubWnd = createMdiSubWindow<SAFigureWindow>(SA::UserChartWnd,chartName);
        SAFigureWindow* pFigWnd = getFigureWidgetFromMdiSubWindow (pSubWnd);
        if(nullptr == pFigWnd)
        {
            return;
        }
        connect (pFigWnd,&SAFigureWindow::chartDataChanged
                 ,this,&MainWindow::onChartDataChanged);
        SAChart2D* pC = pFigWnd->create2DPlot();
        pC->setTitle(chartName);
        pC->setAutoReplot(false);
        QList<QwtPlotCurve*> curList = addChart.getDrawCurveList();
        for(auto ite = curList.begin();ite != curList.end();++ite)
        {
            (*ite)->detach();//先要和原来的脱离连接才能绑定到新图
            pC->addCurve(*ite);
        }
        bool isDateTime = false;
        QString tf = addChart.isAxisDateTime(&isDateTime,QwtPlot::xBottom);
        if(isDateTime)
            pC->setAxisDateTimeScale(tf,QwtPlot::xBottom);
        isDateTime = false;
        tf = addChart.isAxisDateTime(&isDateTime,QwtPlot::yLeft);
        if(isDateTime)
            pC->setAxisDateTimeScale(tf,QwtPlot::yLeft);
        pC->enableZoomer(false);
        pC->enablePicker(false);
        pC->enableGrid(true);
        pC->setAxisTitle(QwtPlot::xBottom,addChart.chart()->axisTitle(QwtPlot::xBottom).text());
        pC->setAxisTitle(QwtPlot::yLeft,addChart.chart()->axisTitle(QwtPlot::yLeft).text());
        pC->enableGrid(true);
        pC->setAutoReplot(true);
        pSubWnd->show();
    }
}
///
/// \brief 趋势图
///
void MainWindow::onActionNewTrendTriggered()
{
    raiseMainDock();
    m_drawDelegate->drawTrend ();
}

///
/// \brief 图表开始矩形选框工具
/// \param b
///
void MainWindow::onActionStartRectSelectTriggered(bool b)
{
    SAChart2D* chart = this->getCurSubWindowChart();
    if(chart)
    {
        if(b)
        {
            chart->startSelectMode(SAChart2D::RectSelection);
            chart->enableZoomer(false);
        }
        else
        {
            chart->stopSelectMode();
        }
    }
    updateChartSetToolBar();
}
///
/// \brief 开始圆形选框工具
/// \param b
///
void MainWindow::onActionStartEllipseSelectTriggered(bool b)
{
    SAChart2D* chart = this->getCurSubWindowChart();
    if(chart)
    {
        if(b)
        {
            chart->startSelectMode(SAChart2D::EllipseSelection);
            chart->enableZoomer(false);
        }
        else
        {
            chart->stopSelectMode();
        }
    }
    updateChartSetToolBar();
}
///
/// \brief 开始多边形选框工具
/// \param b
///
void MainWindow::onActionStartPolygonSelectTriggered(bool b)
{
    SAChart2D* chart = this->getCurSubWindowChart();
    if(chart)
    {
        if(b)
        {
            chart->startSelectMode(SAChart2D::PolygonSelection);
            chart->enableZoomer(false);
        }
        else
        {
            chart->stopSelectMode();
        }
    }
    updateChartSetToolBar();
}
///
/// \brief 清除所有选区
/// \param b
///
void MainWindow::onActionClearAllSelectiedRegion(bool b)
{
    Q_UNUSED(b);
    SAChart2D* chart = this->getCurSubWindowChart();
    if(chart)
    {
        chart->clearAllSelectedRegion();
    }
    updateChartSetToolBar();
}

///
/// \brief 开启当前绘图的十字光标
///
void MainWindow::onActionEnableChartPicker(bool check)
{
    SAFigureWindow* fig = getCurrentFigureWindow();
    if(fig)
    {
        QList<SAChart2D*> charts = fig->get2DPlots();
        std::for_each(charts.begin(),charts.end(),[check](SAChart2D* c){
            c->enablePicker(check);
        });
    }
    else
    {
        ui->actionEnableChartPicker->setChecked(false);
    }
}
///
/// \brief 开启当前绘图的拖动
/// \param check
///
void MainWindow::onActionEnableChartPanner(bool check)
{
    SAFigureWindow* fig = getCurrentFigureWindow();
    if(fig)
    {
        QList<SAChart2D*> charts = fig->get2DPlots();
        std::for_each(charts.begin(),charts.end(),[check](SAChart2D* c){
            c->enablePanner(check);
        });
    }
    else
    {
        ui->actionEnableChartPanner->setChecked(false);
    }
}
///
/// \brief 开启当前绘图的区间缩放
/// \param check
///
void MainWindow::onActionEnableChartZoom(bool check)
{
    SAFigureWindow* fig = getCurrentFigureWindow();
    if(fig)
    {
        QList<SAChart2D*> charts = fig->get2DPlots();
        std::for_each(charts.begin(),charts.end(),[check](SAChart2D* c){
            if(check)
            {
                //选框模式和放大模式是有冲突的。
                c->stopSelectMode();
            }
            c->enableZoomer(check);
        });

    }
    updateChartSetToolBar();
}
///
/// \brief 当前绘图的缩放还原
/// \param check
///
void MainWindow::onActionChartZoomToBase(bool check)
{
    Q_UNUSED(check);
    SAChart2D* chart = this->getCurSubWindowChart();
    if(chart)
        chart->setZoomBase();
}
///
/// \brief 当前绘图放大
/// \param check
///
void MainWindow::onActionChartZoomIn(bool check)
{
    Q_UNUSED(check);
    SAChart2D* chart = this->getCurSubWindowChart();
    if(chart)
        chart->zoomIn();
}
///
/// \brief 当前绘图缩小
/// \param check
///
void MainWindow::onActionChartZoomOut(bool check)
{
    Q_UNUSED(check);
    SAChart2D* chart = this->getCurSubWindowChart();
    if(chart)
        chart->zoomOut();
}
///
/// \brief 当前绘图重置
/// \param check
///
void MainWindow::onActionChartZoomReset(bool check)
{
    Q_UNUSED(check);
    SAChart2D* chart = this->getCurSubWindowChart();
    if(chart)
        chart->setZoomReset();
}



///
/// \brief 清除项目
///
void MainWindow::onActionClearProjectTriggered()
{
    if(QMessageBox::No == QMessageBox::question(this
                                                ,QStringLiteral("通知")
                                                ,QStringLiteral("确认清除项目？\n清楚项目将会把当前所有操作清空")
                                                ))
    {
        return;
    }
    emit cleaningProject();
    saValueManager->clear();
    QList<QMdiSubWindow*> subWindows = ui->mdiArea->subWindowList();
    for (auto ite = subWindows.begin();ite != subWindows.end();++ite)
    {
        SAMdiSubWindow* wnd = qobject_cast<SAMdiSubWindow*>((*ite));
        wnd->askOnCloseEvent(false);
    }
    ui->mdiArea->closeAllSubWindows();
    showNormalMessageInfo(QStringLiteral("清除方案"),0);
    emit cleanedProject();
}





///
/// \brief 子窗口激活
/// \param arg1
///
void MainWindow::onMdiAreaSubWindowActivated(QMdiSubWindow *arg1)
{

    if(nullptr == arg1)
        return;
    if(m_lastActiveWnd == arg1)
        return;
    ui->toolBar_chartSet->setEnabled(true);
    m_lastActiveWnd = arg1;

    SAFigureWindow* fig = getFigureWidgetFromMdiSubWindow(arg1);
    if(fig)
    {
        //窗口激活后，把绘图窗口的指针保存
        m_lastShowFigureWindow = fig;


        //刷新toolbar
        updateChartSetToolBar(fig);

        //更新dock - plotLayer 图层
		SAPlotLayerModel* plotLayerModel = getPlotLayerModel();
		if(plotLayerModel)
		{
            SAChart2D* plot = fig->current2DPlot();
            if(plot)
            {
                plotLayerModel->setPlot(plot);
            }
		}
        QList<SAChart2D*> plotWidgets = getCurSubWindowCharts();
        QList<QwtPlot*> qwtChart;
        std::for_each(plotWidgets.begin(),plotWidgets.end(),[&](SAChart2D* p){
            qwtChart.append(static_cast<QwtPlot*>(p));
        });
        //更新dock - dataviewer
        QwtPlotItemTreeModel* qwtItemTreeModel = getDataViewerPlotItemTreeModel();
        if(qwtItemTreeModel)
        {

            qwtItemTreeModel->setPlots(qwtChart);
            ui->treeView_curPlotItem->expandAll();
        }
        //新窗口激活后，把原来显示的数据clear
        QwtPlotItemDataModel* plotItemDataModel = getDataViewerPlotItemDataModel();
        if(plotItemDataModel)
        {
            plotItemDataModel->clear();
        }
    }
    //设置绘图属性窗口,空指针也接受
    ui->figureSetWidget->setFigureWidget(fig);
}

void MainWindow::onSubWindowClosed(QMdiSubWindow *arg1)
{
    SAFigureWindow* fig = getFigureWidgetFromMdiSubWindow(arg1);
    if(fig)
    {
        //绘图窗口关闭，判断当前记录的绘图窗口指针，若是，就删除
        if(m_lastShowFigureWindow == fig)
        {
            m_lastShowFigureWindow = nullptr;
        }
        //
        getPlotLayerModel()->setPlot(nullptr);
        //窗口关闭,更新dock - dataviewer
        QwtPlotItemTreeModel* qwtItemTreeModel = getDataViewerPlotItemTreeModel();
        if(qwtItemTreeModel)
        {
            qwtItemTreeModel->clear();
        }
        //窗口关闭，把原来显示的数据clear
        QwtPlotItemDataModel* plotItemDataModel = getDataViewerPlotItemDataModel();
        if(plotItemDataModel)
        {
            plotItemDataModel->clear();
        }
    }

    ui->dataFeatureWidget->mdiSubWindowClosed(arg1);
}








///
/// \brief 数据剔除，会把选定区域的数据剔除
///
void MainWindow::onActionInRangDataRemoveTriggered()
{
    SAFigureWindow* wnd = getCurrentFigureWindow();
    if(nullptr == wnd)
    {
        showWarningMessageInfo(tr("you should select a figure window"));
        return;
    }
    SAChart2D* chart = wnd->current2DPlot();
    if(nullptr == chart)
    {
        showWarningMessageInfo(tr("I can not find any chart in figure!"));
        return;
    }
    QList<QwtPlotCurve*> curs = CurveSelectDialog::getSelCurve(chart,this);
    if(curs.size() > 0)
    {
        chart->removeDataInRang(curs);
    }
}


///
/// \brief 隐藏状态栏的进度
///
void MainWindow::hideProgressStatusBar()
{
    ui_status_progress->setVisible(false);
}

///
/// \brief 显示状态栏的进度
///
void MainWindow::showProgressStatusBar()
{
    ui_status_progress->setVisible(true);
}

///
/// \brief 设置状态栏上的进度显示栏是否显示
/// \param isShow
///
void MainWindow::setProgressStatusBarVisible(bool isShow)
{
    ui_status_progress->setVisible(isShow);
}
///
/// \brief 设置状态栏上的进度显示的进度状的百分比
/// \param present
///
void MainWindow::setProgressStatusBarPresent(int present)
{
    ui_status_progress->setPresent(present);
}
///
/// \brief 设置状态栏上的文字
/// \param text
///
void MainWindow::setProgressStatusBarText(const QString &text)
{
    ui_status_progress->setText(text);
}
///
/// \brief 获取进度栏上的进度条指针
/// \return
///
QProgressBar *MainWindow::getProgressStatusBar()
{
    return ui_status_progress->getProgressBar();
}
///
/// \brief 创建一个绘图窗口
/// \note 注意，若没有显示，getCurrentFigureWindow返回的还是上次显示的figureWidget
/// \return 绘图窗口Pointer,如果内存溢出返回nullptr
///
QMdiSubWindow *MainWindow::createFigureWindow(const QString &title)
{
    SAMdiSubWindow* sub = m_drawDelegate->createFigureMdiSubWidget(title);
    return sub;
}

///
/// \brief 获取当前正在显示的图形窗口
/// \return 如果没有或不是显示图形窗口，则返回nullptr
///
SAFigureWindow *MainWindow::getCurrentFigureWindow()
{
    return m_lastShowFigureWindow;
}
///
/// \brief 获取所有的figure
/// \return
///
QList<SAFigureWindow *> MainWindow::getFigureWindowList() const
{
    QList<SAFigureWindow *> res;
    QList<QMdiSubWindow*> subList = ui->mdiArea->subWindowList();
    SAFigureWindow * fig = nullptr;
    for(int i=0;i<subList.size();++i)
    {
        fig = qobject_cast<SAFigureWindow *>(subList[i]->widget());
        if(fig)
        {
            res.append(fig);
        }
    }
    return res;
}
///
/// \brief 获取当前正在显示的Chart指针
/// \return 如果没有或不是显示chart，则返回nullptr
///
SAChart2D *MainWindow::getCurSubWindowChart()
{
    SAFigureWindow* pBase = getCurrentFigureWindow();
    if (nullptr == pBase)
        return nullptr;
    return pBase->current2DPlot();
}
///
/// \brief 获取当前正在显示的Chart指针,如果是一个subplot，返回多个指针
/// \param 如果没有或不是显示chart，则返回nullptr
/// \return
///
QList<SAChart2D*> MainWindow::getCurSubWindowCharts()
{
    SAFigureWindow* pFig = getCurrentFigureWindow();
    if (nullptr == pFig)
        return QList<SAChart2D *>();
    return pFig->get2DPlots();
}
///
/// \brief 用于子窗口激活时刷新“图表设置工具栏的选中状态”
///
void MainWindow::updateChartSetToolBar(SAFigureWindow *w)
{
    if(nullptr == w)
    {
        w = this->getCurrentFigureWindow();
    }
    if(nullptr == w)
    {
        return;
    }
    auto c = w->current2DPlot();
    if(c)
    {
        ui->actionEnableChartPicker->setChecked( c->isEnablePicker() );
        ui->actionEnableChartPanner->setChecked( c->isEnablePanner() );
        ui->actionEnableChartZoom->setChecked(c->isEnableZoomer());
        ui->actionYDataPicker->setChecked(c->isEnableYDataPicker());
        ui->actionXYDataPicker->setChecked(c->isEnableXYDataPicker());
        ui->actionShowGrid->setChecked(c->isEnableGrid());
        ui->actionShowHGrid->setChecked(c->isEnableGridY());
        ui->actionShowVGrid->setChecked(c->isEnableGridX());
        ui->actionShowCrowdedHGrid->setChecked(c->isEnableGridYMin());
        ui->actionShowCrowdedVGrid->setChecked(c->isEnableGridXMin());
        ui->actionShowLegend->setChecked(c->isEnableLegend());
        ui->actionLegendPanel->setChecked(c->isEnableLegendPanel());
        SAChart2D::SelectionMode selMode = c->currentSelectRegionMode();
        ui->actionStartRectSelect->setChecked(SAChart2D::RectSelection == selMode);
        ui->actionStartEllipseSelect->setChecked(SAChart2D::EllipseSelection == selMode);
        ui->actionStartPolygonSelect->setChecked(SAChart2D::PolygonSelection == selMode);
    }
}

QList<QMdiSubWindow *> MainWindow::getSubWindowList() const
{
    return ui->mdiArea->subWindowList();
}
///
/// \brief 从subwindow指针中查找是否含有SAFigureWindow
/// \param sub subwindow指针
/// \return 存在返回指针，否则返回nullptr
///
SAFigureWindow*MainWindow::getFigureWidgetFromMdiSubWindow(QMdiSubWindow* sub)
{
    return qobject_cast<SAFigureWindow*>(sub->widget());
}

///
/// \brief 获取当前激活的子窗口
/// \return 如果窗口没有激活，返回nullptr
///
QMdiSubWindow *MainWindow::getCurrentActiveSubWindow() const
{
    return qobject_cast<SAMdiSubWindow*>(ui->mdiArea->activeSubWindow());
}
///
/// \brief 判断mdi中是否存在指定的子窗口
/// \param
/// \return
///
bool MainWindow::isHaveSubWnd(QMdiSubWindow* wndToCheck) const
{
    if (nullptr == wndToCheck)
        return false;
    QList<QMdiSubWindow*> subs = ui->mdiArea->subWindowList();
    for (auto ite = subs.begin();ite != subs.end();++ite)
    {
        if (*ite == wndToCheck)
        {
            return true;
        }
    }
    return false;
}
///
/// \brief 把主dock抬起，主dock包括绘图的窗口
///
void MainWindow::raiseMainDock()
{
    ui->dockWidget_main->raise();
}
///
/// \brief 把信息窗口抬起
///
void MainWindow::raiseMessageInfoDock()
{
    ui->dockWidget_message->raise();
}
///
/// \brief 如果插件只导入到data import菜单下的条目可以调用此函数，如果需要插入一个QMenu可以使用addDataImportPluginAction
/// \param action
///
void MainWindow::addDataImportPluginAction(QAction *action)
{
    ui->menu_import->addAction(action);
}
///
/// \brief 添加导入数据插件菜单
/// \param menu
/// \return
///
QAction *MainWindow::addDataImportPluginMenu(QMenu *menu)
{
    return ui->menu_import->addMenu(menu);
}
///
/// \brief 把菜单添加到分析功能的菜单中
/// \param menu
/// \return
///
QAction *MainWindow::addAnalysisPluginMenu(QMenu *menu)
{
    return ui->menu_Analysis->addMenu(menu);
}


///
/// \brief 在ui界面显示普通信息
/// \param info 信息内容
/// \param interval 如果此参数大于0，将会在状态栏也弹出提示信息
/// \see showErrorInfo showWarningInfo showQuestionInfo showMessageInfo showWidgetMessageInfo
///
void MainWindow::showNormalMessageInfo(const QString& info, int interval)
{
    ui->saMessageWidget->addStringWithTime(info,Qt::black);
    if(interval > 0)
        ui_status_info->showNormalInfo (info,interval);
}

///
/// \brief 错误信息警告
/// \param info 信息内容
/// \param interval 如果此参数大于0，将会在状态栏也弹出提示信息
/// \see showNormalInfo showWarningInfo showQuestionInfo showMessageInfo showWidgetMessageInfo
///
void MainWindow::showErrorMessageInfo(const QString& info, int interval)
{
    ui->saMessageWidget->addStringWithTime(info,Qt::red);
    if(interval > 0)
        ui_status_info->showErrorInfo (info,interval);
}
///
/// \brief 在ui界面显示警告信息
/// \param info 信息内容
/// \param interval 如果此参数大于0，将会在状态栏也弹出提示信息
/// \see showNormalInfo showErrorInfo showQuestionInfo showMessageInfo showWidgetMessageInfo
///
void MainWindow::showWarningMessageInfo(const QString& info, int interval)
{
    QColor clr(211,129,10);
    ui->saMessageWidget->addStringWithTime(info,clr);
    if(interval > 0)
        ui_status_info->showWarningInfo (info,interval);
}
///
/// \brief 在ui界面显示询问信息
/// \param info 信息内容
/// \param interval 如果此参数大于0，将会在状态栏也弹出提示信息
/// \see showNormalInfo showErrorInfo showWarningInfo showMessageInfo showWidgetMessageInfo
///
void MainWindow::showQuestionMessageInfo(const QString& info, int interval)
{
    ui->saMessageWidget->addStringWithTime(info,Qt::blue);
    if(interval > 0)
        ui_status_info->showQuesstionInfo (info,interval);
}
///
/// \brief 把消息显示到窗口中
/// \param info 消息
/// \param messageType 需要显示的消息类型 \sa SA::MeaasgeType
/// \see showNormalInfo showErrorInfo showWarningInfo showQuestionInfo showWidgetMessageInfo
///
void MainWindow::showMessageInfo(const QString &info, SA::MeaasgeType messageType)
{
    showWidgetMessageInfo(info,nullptr,messageType,-1);
}
///
/// \brief 接收SAWidget发出的信息
/// \param info 信息内容
/// \param widget 窗口指针,如果不是窗口发送，指定nullptr即可
/// \param messageType 信息类型 \sa SA::MeaasgeType
/// \param interval 信息显示时间
/// \see showNormalInfo showErrorInfo showWarningInfo showQuestionInfo showMessageInfo
///
void MainWindow::showWidgetMessageInfo(const QString& info, QWidget* widget, SA::MeaasgeType messageType, int interval)
{
    if(nullptr != widget)
    {
        ui->saMessageWidget->addString(widget->windowTitle ());
    }
    switch(messageType)
    {
    case SA::NormalMessage:
    {
        showNormalMessageInfo(info,interval);
        break;
    }
    case SA::ErrorMessage:
    {
        showErrorMessageInfo(info,interval);
        break;
    }
    case SA::QuesstionMessage:
    {
        showQuestionMessageInfo(info,interval);
        break;
    }
    case SA::WarningMessage:
    {
        showWarningMessageInfo(info,interval);
        break;
    }
    default:
        showNormalMessageInfo(info,interval);
    }
}




QIcon MainWindow::getIconByWndType(SA::SubWndType type)
{
    switch(type)
    {
        case SA::UserChartWnd:
        case SA::TendencyChartWnd:
            return ICON_UserChart;
        case SA::ThermalDiagramWnd:
            return ICON_ThermalTS;
        case SA::SignalChartWnd:
            return ICON_Spectrum;
        case SA::ThermalPropertiesWnd:
            return ICON_ThermalProperties;
        default:return QIcon();
    }
    return QIcon();
}

///
/// \brief 提取图表中曲线的数据到sa中，以便用于其他的分析
/// \see pickCurData
///
void MainWindow::onActionPickCurveToDataTriggered()
{
    SAChart2D* chart = getCurSubWindowChart();
    if(!chart)
    {
        showWarningMessageInfo(tr("you should select a figure window"));
        return;
    }
    QList<QwtPlotCurve*> curs = CurveSelectDialog::getSelCurve(chart,this);
    if(curs.size()==0)
        return;
    PickCurveDataModeSetDialog pcDlg(this);
    if(QDialog::Accepted !=  pcDlg.exec())
        return;
    SA::ViewRange rang = pcDlg.getViewRange();
    SA::PickDataMode pickMode = pcDlg.getPickDataMode();
    for(auto i=curs.begin();i!=curs.end();++i)
    {
        QString name = QInputDialog::getText(this,QStringLiteral("数据命名")
                                             ,QStringLiteral("请为导出的数据命名："),QLineEdit::Normal,(*i)->title().text());
        if(SA::InViewRange == rang)
            pickCurData(name,(*i),pickMode,chart->getPlottingRegionRang());
        else
            pickCurData(name,(*i),pickMode,QRectF());
    }
}
///
/// \brief 在当前标签中显示数据内容
///
void MainWindow::onActionViewValueInCurrentTabTriggered()
{
    QList<SAAbstractDatas*> datas=getSeletedDatas();
    if(datas.size ()<=0)
        return;
    ui->tabWidget_valueViewer->setDataInCurrentTab(datas);
}
///
/// \brief 在新标签中显示数据内容
///
void MainWindow::onActionViewValueInNewTabTriggered()
{
    QList<SAAbstractDatas*> datas=getSeletedDatas();
    if(datas.size ()<=0)
        return;
    ui->tabWidget_valueViewer->setDataInNewTab(datas);
}
///
/// \brief action 变量重命名
///
void MainWindow::onActionRenameValueTriggered()
{ 
    SAAbstractDatas* data = getSeletedData();
    if(data)
    {
        bool ok=false;
        QString name = QInputDialog::getText(this,QStringLiteral("输入新变量名")
                                             ,QStringLiteral("新变量名"),QLineEdit::Normal
                                             ,data->getName(),&ok);
        if(!ok)
            return;
        if(name == data->getName())
            return;
        if(name.isNull() || name.isEmpty())
        {
            QMessageBox::warning(this,QStringLiteral("警告"),QStringLiteral("请输入有效的变量名"));
            return;
        }
        saValueManager->renameData(data,name);
    }
}



///
/// \brief 获取选择的数据条目
///
/// 若在没有选中时需要用户显示选择，使用getSelectSingleData函数
/// \return 若没有在变量管理获取焦点，返回nullptr
/// \see getSelectSingleData
///
SAAbstractDatas* MainWindow::getSeletedData() const
{
    return ui->treeView_valueManager->getSeletedData();
}
///
/// \brief 获取选中的数据条目，如果没有选中，将弹出数据选择窗口让用户进行选择
/// \param isAutoSelect 若为true时，会先查找变量管理器是否选中了数据，若选中了，直接返回选中的数据，
/// 默认为false
/// \return 若用户取消，返回nullptr
/// \see getSeletedData
///
SAAbstractDatas *MainWindow::getSelectSingleData(bool isAutoSelect)
{
    SAAbstractDatas *data = nullptr;
    data = getSeletedData();
    if(isAutoSelect)
    {
        if(data)
        {
            return data;
        }
    }
    QList<SAAbstractDatas*> tmp;
    SAValueSelectDialog dlg(this);
    dlg.setSelectMode(SAValueSelectDialog::SingleSelection);
    dlg.setDefaultSelectDatas({data});
    if(QDialog::Accepted == dlg.exec())
    {
        tmp = dlg.getSelectDatas();
    }

//    QList<SAAbstractDatas*> tmp
//            = SAValueSelectDialog::getSelectDatas(this,SAValueSelectDialog::SingleSelection);
    if(0==tmp.size())
    {
        return nullptr;
    }
    return tmp[0];
}


///
/// \brief 获取选择的数据条目，此方法会自动调整选中的内容，如果选择的是说明，也会自动更改为对应的内容
/// \param index
/// \return
///
QList<SAAbstractDatas*> MainWindow::getSeletedDatas() const
{
    return ui->treeView_valueManager->getSeletedDatas();
}


SAPlotLayerModel* MainWindow::getPlotLayerModel() const
{
	return static_cast<SAPlotLayerModel*>(ui->tableView_layer->model());
}
///
/// \brief 抽取曲线的数据到openDataManager
/// \param name 曲线数据名
/// \param cur 曲线指针
/// \param pickMode 抽取方式
/// \param rang 抽取范围
///
void MainWindow::pickCurData(const QString& name, QwtPlotCurve *cur, SA::PickDataMode pickMode, const QRectF& rang)
{
    std::shared_ptr<SAAbstractDatas> p = nullptr;
    if(SA::XOnly == pickMode)
    {
        QVector<double> x;
        SAChart::getXDatas(x,cur,rang);
        p = SAValueManager::makeData<SAVectorDouble>(name,x);//new SAVectorDouble(name,x);

    }
    else if(SA::YOnly == pickMode)
    {
        QVector<double> y;
        SAChart::getYDatas(y,cur,rang);
        p = SAValueManager::makeData<SAVectorDouble>(name,y);//new SAVectorDouble(name,y);
    }
    else if(SA::XYPoint == pickMode)
    {
        QVector<QPointF> xy;
        SAChart::getXYDatas(xy,cur,rang);
        p = SAValueManager::makeData<SAVectorPointF>(name,xy);//new SAVectorPointF(name,xy);
    }
    saValueManager->addData(p);
}




#include <AboutDialog.h>
void MainWindow::onActionAboutTriggered()
{
    AboutDialog aboutDlg;
    aboutDlg.exec ();
}


///
/// \brief 显示延时状态栏信息
/// 延时信息显示在状态栏里，不同的信息类型显示不同的图标
/// \param info 信息内容
/// \param type 信息类型
/// \param interval 延时时间，默认4s
///
void MainWindow::showElapesdMessageInfo(const QString& info, SA::MeaasgeType type, int interval)
{
    ui_status_info->postInfo (info,type,interval);
}


///
/// \brief Rescind （回退）
///
void MainWindow::onActionRescindTriggered()
{
    saValueManager->getUndoStack()->undo();
}
///
/// \brief Redo （重做）
///
void MainWindow::onActionRedoTriggered()
{
    saValueManager->getUndoStack()->redo();
}

///
/// \brief 图层视图的点击
/// \param index
///
void MainWindow::onTableViewLayerPressed(const QModelIndex &index)
{
    if(!index.isValid ())
        return;
    QColor rgb = index.data (Qt::BackgroundColorRole).value<QColor>();
    ui->tableView_layer->setStyleSheet (getPlotLayerNewItemSelectedQSS(rgb));
    SAPlotLayerModel* model=getPlotLayerModel();
    if (1==index.column())
    {
        QwtPlotItem* item = model->getPlotItemFromIndex (index);
        model->setData (index,!item->isVisible ());

    }
    else if(index.column() == 2)
    {//颜色
        QColorDialog clrDlg;
        clrDlg.setCurrentColor(rgb);
        if(clrDlg.exec() == QDialog::Accepted)
        {
            QColor newClr = clrDlg.selectedColor();
            ui->tableView_layer->setStyleSheet (getPlotLayerNewItemSelectedQSS(newClr));
            model->setData (index,newClr,Qt::BackgroundColorRole);
        }
    }

}
///
/// \brief 根据已有的背景颜色来设置选中状态的背景颜色
/// \param rgb 已有的背景颜色
/// \return 设置的qss字符串
///
QString MainWindow::getPlotLayerNewItemSelectedQSS(const QColor& rgb)
{
    QColor fgrgb;
    //qDebug()<<"rgb:"<<rgb<<" lightness:"<<rgb.lightness()<<" value:"<<rgb.value();
    if(rgb.lightness()<128)//根据背景颜色的深浅设置前景颜色
    {
        fgrgb.setRgb (255,255,255);
    }
    else
    {
        fgrgb.setRgb (0,0,0);
    }
    QString qss = QString("QTableView::item:selected{selection-background-color:rgb(%1,%2,%3);}"
                          "QTableView::item:selected{selection-color:rgb(%4,%5,%6);}")
                  .arg (rgb.red ()).arg (rgb.green ()).arg (rgb.blue ())
                  .arg (fgrgb.red ()).arg (fgrgb.green ()).arg (fgrgb.blue ());//文字用反色
    return qss;
}

bool MainWindow::setProjectInfomation()
{
    SAProjectInfomationSetDialog dlg(this);
    dlg.setProjectName(saProjectManager->getProjectName());
    dlg.setProjectDescribe(saProjectManager->getProjectDescribe());
    if(QDialog::Accepted != dlg.exec())
    {
        return false;
    }
    saProjectManager->setProjectName(dlg.getProjectName());
    saProjectManager->setProjectDescribe(dlg.getProjectDescribe());
    return true;
}
///
/// \brief 变量管理器的移除控制触发的槽
/// \param dataBeDeletedPtr 移除控制的数据
///
void MainWindow::onDataRemoved(const QList<SAAbstractDatas *> &dataBeDeletedPtr)
{
    ui->tabWidget_valueViewer->removeDatas(dataBeDeletedPtr);
}





QwtPlotItemTreeModel *MainWindow::getDataViewerPlotItemTreeModel() const
{
    return static_cast<QwtPlotItemTreeModel*>(ui->treeView_curPlotItem->model());
}

QwtPlotItemDataModel *MainWindow::getDataViewerPlotItemDataModel() const
{
    return static_cast<QwtPlotItemDataModel*>(ui->tableView_curSelItemDatas->model());
}

///
/// \brief 曲线条目树形窗口
/// \param index
///
void MainWindow::onTreeViewCurPlotItemClicked(const QModelIndex &index)
{
    Q_UNUSED(index);
    QItemSelectionModel* sel = ui->treeView_curPlotItem->selectionModel();
    QModelIndexList indexList = sel->selectedRows();
    QwtPlotItemTreeModel* treeModel = getDataViewerPlotItemTreeModel();
    QwtPlotItemDataModel* tableModel = getDataViewerPlotItemDataModel();
    if(!treeModel)
        return;
    QList<QwtPlotItem*> items;
    for(int i = 0;i<indexList.size ();++i)
    {
        if(!indexList[i].parent().isValid())
        {//说明点击的是父窗口，就是qwtplot,这时显示所有
            items.clear();
            int childIndex = 0;
            while (indexList[i].child(childIndex,0).isValid()) {
                items.append(treeModel->getQwtPlotItemFromIndex (
                                 indexList[i].child(childIndex,0)));
                ++childIndex;
            }
            break;
        }
        if(indexList[i].column () != 0)
        {
            indexList[i] = indexList[i].parent().child(indexList[i].row(),0);
        }
        items.append (treeModel->getQwtPlotItemFromIndex (indexList[i]));
    }
    tableModel->setQwtPlotItems (items);
}




///
/// \brief 删除变量
///
void MainWindow::onActionDeleteValueTriggered()
{
    QList<SAAbstractDatas*> datas = getSeletedDatas();
    QString info=tr("Are you sure remove:\n");
    for(int i=0;i<datas.size () && i < 5;++i)
    {
        info += QStringLiteral("%1;").arg(datas[i]->text ());
    }
    info += tr("\n datas?");
    QMessageBox::StandardButton btn = QMessageBox::question (this,tr("Quesstion"),info);
    if(QMessageBox::Yes != btn)
    {
        return;
    }
    saValueManager->removeDatas(datas);
}






