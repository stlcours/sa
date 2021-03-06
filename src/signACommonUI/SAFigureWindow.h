#ifndef SAFIGUREWINDOW_H
#define SAFIGUREWINDOW_H

#include <QMainWindow>
#include <QScopedPointer>
#include "SAAbstractFigure.h"
#include "SACommonUIGlobal.h"

#include "qwt_plot_histogram.h"
class QwtPlotCurve;
class QwtPlotItem;

class SAAbstractDatas;
class SAVectorInterval;
class DataFeaturePlotInfoItem;
class SAChart2D;
class SAFigureContainer;
class CurveDataFeatureItem;

///
/// \brief SA的绘图窗口
///
class SA_COMMON_UI_EXPORT SAFigureWindow : public QMainWindow
        ,public SAAbstractFigure
{
    Q_OBJECT
public:
    explicit SAFigureWindow(QWidget *parent = 0);
    ~SAFigureWindow();
    //添加一个2D chart
    SAChart2D* create2DPlot();
    SAChart2D* create2DSubPlot(float xPresent, float yPresent, float wPresent, float hPresent);
    //获取所有的图表
    QList<SAChart2D*> get2DPlots() const;
    //获取当前的2d绘图指针
    SAChart2D* current2DPlot() const;

    //设置画布背景色
    void setBackgroundColor(const QBrush& brush);
    void setBackgroundColor(const QColor& clr);
signals:
    //绘图数据变更消息
    void chartDataChanged(QwtPlot* plot,QwtPlotItem* item);
private:
    void initUI();
private:
    class UI
    {
    public:
        void setupUI(SAFigureWindow* par);
        void retranslateUi(SAFigureWindow* par);
        SAFigureContainer *centralwidget;
    };

private:
    QScopedPointer<UI> ui;
    SAChart2D *m_currentPlot;
};

#endif // SAFIGUREWINDOW_H
