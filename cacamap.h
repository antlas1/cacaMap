#ifndef CACAMAP_H
#define CACAMAP_H

#include <QtGui>
#include <QtNetwork>
#include <QWidget>
#include <QSlider>
#include <QHBoxLayout>


struct tileserver
{
    QString name;/**<name of the tile server*/
    QString url;/**< url template for accessing tiles*/
    QString folder;/**< name of folder where tiles will be stored*/
    QString path;/**< path where tiles will be stored*/
    QString tile;/**< tile file*/
};

class servermanager
{
public:
    servermanager();
    QString getTileUrl(int,quint32,quint32);
    QString tileCacheFolder();
    //returns the filename of the file as it should be stored in HD
    QString fileName(quint32);
    QString serverName();
    QString filePath(int, quint32);

private:
    tileserver servermain;
};

/**
* The quint32 version of QPoint
*/

struct longPoint
{
	quint32 x;/**< x coord. */
	quint32 y;/**< y coord.*/
	longPoint(quint32,quint32);
	longPoint();
};

/**
Helper struct that handles coordinate transformations
*/
struct myMercator
{
	static longPoint geoCoordToPixel(QPointF const &,int , int);
	static QPointF pixelToGeoCoord(longPoint const &, int, int);
};
/**
* Struct to define a range of consecutive tiles
* It's used to identify which tiles are visible and need to be rendered/downlaoded
* @see cacaMap::updateTilesToRender()
*/
struct tileSet
{
	int zoom;/**< zoom level.*/
	qint32 top;/**< topmost row.*/
	qint32 bottom;/**< bottommost row.*/
	qint32 left;/**< leftmostcolumn. */
	qint32 right;/**< rightmost column. */
	int offsetx;/**< horizontal offset needed to align the tiles in the wiget.*/
	int offsety;/**< vertical offset needed to align the tiles in the widget.*/
};

/**
* Used to represent a specific %tile
* @see cacaMap::tileCache
*/
struct tile
{
	int zoom;/**< zoom level.*/
	qint32 x;/**< colum number.*/
	qint32 y;/**< row number.*/
	QString  url;/**<used to identify the %tile when it finishes downloading.*/
};
/**
* maximum space allowed for caching tiles
*/
#define CACHE_MAX 100*1024*1024 //100 MB
/**
Main map widget
*/


class cacaMap : public QWidget
{

Q_OBJECT

public:	
    cacaMap(QPointF startcoords,
            bool enable_download,
            QWidget * _parent=0);

	virtual ~cacaMap();

	bool zoomIn();
	bool zoomOut();
	bool setZoom(int level);
    int getZoom();

    void setGeoCoords(QPointF);
    QPointF getGeoCoords();

    void setEnableDownloadTiles(bool enabled);
    bool enabledDownloadTiles() const;
private:
	QNetworkAccessManager *manager;/**< manages http requests. */
	tileSet tilesToRender;/**< range of visible tiles. */
	QHash<QString,int> tileCache;/**< list of cached tiles (in HDD). */
	QHash<QString,tile> downloadQueue;/**< list of tiles waiting to be downloaded. */
	QHash<QString,int> unavailableTiles;/**< list of tiles that were not found on the server.*/
    bool enable_downloading;
    bool downloading;/**< flag that indicates if there is a download going on. */
	QString folder;/**< root application folder. */
	QMovie loadingAnim;/**< to show a 'loading' animation for yet unavailable tiles. */
	QPixmap notAvailableTile;
	servermanager servermgr;	

	void renderMap(QPainter &);
	void downloadPicture();
	void loadCache();
	QString getTilePath(int, qint32);
	QPixmap getTilePatch(int,quint32,quint32,int,int,int);

protected:
	int zoom;/**< Map zoom level. */
	int minZoom;/**< Minimum zoom level (farthest away).*/
	int maxZoom;/**< Maximum zoom level (closest).*/

    const int tileSize; /**< size in px of the square %tile. */
	quint32 cacheSize;/**< current %tile cache size in bytes. */
	//check QtMobility QGeoCoordinate
	QPointF geocoords; /**< current longitude and latitude. */
	QPixmap* imgBuffer;
	QPixmap tmpbuff;
	float buffzoomrate;

	bool bufferDirty; /**< image buffer needs to be updated. */	
	void resizeEvent(QResizeEvent*);
	void paintEvent(QPaintEvent *);
	void updateTilesToRender();
	void updateBuffer();
	void updateContent();

protected slots:
	void slotDownloadProgress(qint64, qint64);
	void slotDownloadReady(QNetworkReply *);
	void slotError(QNetworkReply::NetworkError);
};



class cacaMapMouse: public cacaMap
{
    Q_OBJECT
public:
    cacaMapMouse(QPointF startcoords,
                 bool enable_download,
                 QWidget* _parent=0);
    ~cacaMapMouse();
protected:
    void paintEvent(QPaintEvent *);
    void mousePressEvent(QMouseEvent*);
    void mouseMoveEvent(QMouseEvent*);
    void mouseDoubleClickEvent(QMouseEvent*);
private:
    QPoint mouseAnchor;/**< used to keep track of the last mouse click location.*/
    QTimer * timer;
    QHBoxLayout * hlayout;

    QSlider * slider;
    QPointF destination; /**< used for dblclick+zoom animations */
    float mindistance;/**< used to identify the end of the animation*/
    float animrate;
protected slots:
    void zoomAnim();
    void updateZoom(int);
};

#endif
