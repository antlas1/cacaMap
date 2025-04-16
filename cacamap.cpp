#include "cacamap.h"
#include <iostream>

using namespace std;

servermanager::servermanager()
{
    tileserver serveritem;

    serveritem.name = "OSM cahced";
    //serveritem.url = "http://a.tile.openstreetmap.org/%z/%x/%y.png";
    serveritem.url = "http://mt1.google.com/vt/x=%x&y=%y&z=%z";
    serveritem.folder = "map_cache";
    serveritem.path = "/%z/%x/";
    serveritem.tile = "%y.png";

    servermain = serveritem;
}


/**
* Get URL of a specific %tile
* @param zoom zoom level
* @return string containing the url where the %tile image can be found.
*/
QString servermanager::getTileUrl(int zoom, quint32 x, quint32 y)
{
    QString sz,sx,sy;
    sz.setNum(zoom);
    sx.setNum(x);
    sy.setNum(y);
    QString urltmpl = servermain.url;

    urltmpl.replace(QString("%z"),sz);
    urltmpl.replace(QString("%x"),sx);
    urltmpl.replace(QString("%y"),sy);
    return urltmpl;
}

/**
* @return name of the cache folder for the given tile server
*/
QString servermanager::tileCacheFolder()
{
    return  servermain.folder;
}

/**
* @return tile file name
*/
QString servermanager::fileName(quint32 y)
{
    QString filetmpl = servermain.tile;
    QString sy;
    sy.setNum(y);
    filetmpl.replace("%y",sy);
    return filetmpl;
}

/**
* @return tile file path
*/
QString servermanager::filePath(int zoom, quint32 x)
{
    QString filetmpl = servermain.path;
    QString sz, sx;
    sz.setNum(zoom);
    sx.setNum(x);
    filetmpl.replace("%z",sz);
    filetmpl.replace("%x",sx);
    return filetmpl;
}


/**
* @return server name
*/
QString servermanager::serverName()
{
    return servermain.name;
}
/**
* constructor
*/
longPoint::longPoint(quint32 _x, quint32 _y)
{
	x = _x;
	y = _y;
}
/**
*empty constructor
*/

longPoint::longPoint()
{
	x = 0;
	y = 0;
}

/**
* Converts a geo coordinate to map pixels
* @param geocoord has the longitude and latitude in degrees.
* @param zoom is the zoom level, ranges from cacaMap::minZoom (out) to maxZoom (in).
* @param tilesize the width/height in px of the square %tile (e.g 256).
* @return a longpoint struct containing the x and y px coordinates
* in the map for the given geocoordinates and zoom level.
*/
longPoint myMercator::geoCoordToPixel(QPointF const &geocoord, int zoom, int tilesize)
{
 	qreal  longitude = geocoord.x();
	qreal  latitude = geocoord.y();
	
	//height, width of the whole map,this is, all tiles for a given zoom level put together
	quint32 mapsize =  (1<<zoom)*tilesize;
	
	qreal latitude_m = atanh(sin(latitude/180.0*M_PI))*180.0/M_PI;

	quint32 x = mapsize*(longitude + 180.0)/360.0;
	
	quint32 y = mapsize*(180.0 - latitude_m)/360.0;
	
	return longPoint(x,y);
}
/**
* Converts  map pixels to geo coordinates in degrees
* @param pixelcoord has the x and y px coordinates.
* @param zoom  is the zoom level, ranges from 0(out) to 18(in).
* @param tilesize the width/height in px of the square %tile (e.g 256).
* @return a QPointF object containing the latitude and longitude of 
* of the given location.
*/

QPointF myMercator::pixelToGeoCoord(longPoint const &pixelcoord, int zoom, int tilesize)
{
	long  x= pixelcoord.x;
	long  y= pixelcoord.y;
	
	//height, width of the whole map,this is, all tiles for a given zoom level put together
	quint32 mapsize =  (1<<zoom)*tilesize;

	
	qreal longitude = x*360.0/mapsize - 180.0;
	
	qreal latitude_m = 180.0 - y*360.0/mapsize;
	qreal latitude = asin(tanh(latitude_m*M_PI/180.0))*180/M_PI;
	
	return QPointF(longitude,latitude);
}
/**
* constructor
*/
cacaMap::cacaMap(QPointF startcoords,
                 bool enable_download,
                 QWidget* parent):QWidget(parent), tileSize(256), enable_downloading(enable_download)
{
	cacheSize = 0;
	maxZoom = 18;
	minZoom = 0;
	folder = QDir::currentPath();
	loadCache();
    geocoords = startcoords;
	downloading = false;
	zoom = 14;
	manager = new QNetworkAccessManager(this);
    manager->setStrictTransportSecurityEnabled(false);
    manager->setRedirectPolicy(QNetworkRequest::SameOriginRedirectPolicy);
    //manager->setRedirectPolicy();
	loadingAnim.setFileName("loading.gif");
	loadingAnim.setScaledSize(QSize(tileSize,tileSize));
	loadingAnim.start();
	notAvailableTile.load("notavailable.jpeg");
	imgBuffer = new QPixmap(size());
	buffzoomrate = 1.0;
}

/**
Sets the latitude and longitude to the coords in newcoords
@param newcoords the new coordinates.
*/
void cacaMap::setGeoCoords(QPointF newcoords)
{
	geocoords = newcoords;
}
/**
* zooms in one level
* @return true if it zoomed succesfully, false otherwise (if maxZoom has been reached)
*/
bool cacaMap::zoomIn()
{
	if (zoom < maxZoom)
	{
		zoom++;
		downloadQueue.clear();
		updateContent();
		return true;
	}
	return false;
}
/**
* zooms out one level
* @return true if it zoomed succesfully, false otherwise (if minZoom has been reached)
*/
bool cacaMap::zoomOut()
{
	if (zoom > minZoom)
	{
		zoom--;
		downloadQueue.clear();
		updateContent();
		return true;
	}
	return false;
}

/**
* zooms to a specific level
* @return true if it is a valid level, false otherwise (if level is outside the valid range)
*/
bool cacaMap::setZoom(int level)
{
	if (level>= minZoom && level <= maxZoom)
	{
		zoom = level;
		downloadQueue.clear();
		updateContent();
		return true;
	}
	return false;
}

/**
* @return current geocoords
*/
QPointF cacaMap::getGeoCoords()
{
	return geocoords;
}

void cacaMap::setEnableDownloadTiles(bool enabled)
{
    enable_downloading = enabled;
    if (enable_downloading == false)
    {
        downloadQueue.clear();
    }
    //TODO: signal
}

bool cacaMap::enabledDownloadTiles() const
{
    return enable_downloading;
}
/**
*   @return current zoom level
*/
int cacaMap::getZoom()
{
	return zoom;
}	

/**
*@param zoom zoom level
*@param x tile x column
*@return string with the path to the folder containing the tiles in 
*for zoom level and x column
*/
QString cacaMap::getTilePath(int zoom,qint32 x)
{
    return servermgr.tileCacheFolder()+servermgr.filePath(zoom,x);
}

/**
* @return image for temporarily replacing a tile that is downloading and currently unavailable
* The 'patch' is a subsection of an available tile from a lower zoom level.
* The algorithm tries to find a suitable tile starting from level zoom -1, until level 0.
* The higher the zoom level difference the more pixelated the patch will be.
*/
QPixmap cacaMap::getTilePatch(int zoom, quint32 x, quint32 y, int offx, int offy, int tsize)
{
	//dont go beyond level 0 and 
	//dont use patches smaller than 16 px. They are unintelligible anyways.
	if (zoom>0 && tsize>=16*2)
	{
		int parentx, parenty, offsetx, offsety;
		QString tileid;
		QPixmap patch;
		QString sz,sx,sy;
		parentx = x/2;
		parenty = y/2;
		sz.setNum(zoom-1);
		sx.setNum(parentx);
		sy.setNum(parenty);
		offsetx = offx/2 + (x%2)*tileSize/2;
		offsety = offy/2 + (y%2)*tileSize/2;
		tileid = sz+"."+sx+"."+sy;
		if (tileCache.contains(tileid))
		{
			//render the tile
			QDir::setCurrent(folder);
			QString path= getTilePath(zoom-1,parentx) ;
			QString fileName = servermgr.fileName(parenty);
			QDir::setCurrent(path);
			QFile f(fileName);
			if (f.open(QIODevice::ReadOnly))
			{
				patch.loadFromData(f.readAll());
				f.close();
				return patch.copy(offsetx,offsety,tsize/2,tsize/2).scaledToHeight(tileSize);
			}
			else
			{
				cout<<"no file found "<<path.toStdString()<<fileName.toStdString()<<endl;
			}
		}
		else
		{
			return getTilePatch(zoom-1,parentx,parenty,offsetx,offsety,tsize/2);
		}
	}
	return loadingAnim.currentPixmap();
}



/**
Starts downloading the next %tile in the queue
@see cacaMap::downloadQueue
*/
void cacaMap::downloadPicture()
{
	//check if there isnt an active download already
	if (!downloading)
	{
		//there are items in the queue
		if (downloadQueue.size())
		{
			downloading = true;
			QHash<QString,tile>::const_iterator i;
			i = downloadQueue.constBegin();
			tile nextItem = i.value();
			QString surl = nextItem.url;
			QNetworkRequest request;
			request.setUrl(QUrl(surl));
			QNetworkReply *reply = manager->get(request);
            connect(reply, SIGNAL(errorOccurred(QNetworkReply::NetworkError)),this, SLOT(slotError(QNetworkReply::NetworkError)));
			connect(manager, SIGNAL(finished(QNetworkReply*)),this, SLOT(slotDownloadReady(QNetworkReply*)));
			connect(reply, SIGNAL(downloadProgress(qint64,qint64)),this, SLOT(slotDownloadProgress(qint64, qint64)));
		}
		else
		{
		//	cout<<"no items in the queue"<<endl;
		}
	}
	else
	{
		cout<<"another download is already in progress... "<<endl;
	}
}
/**
Populates the cache list by checking the existing files on the cache folder
*/
void cacaMap::loadCache()
{
	cacheSize=0;
	unavailableTiles.clear();
	tileCache.clear();
	QDir::setCurrent(folder);
	QDir dir;
    if (dir.cd(servermgr.tileCacheFolder()))
    {
        QStringList zoom = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
        QString zoomLevel;
        for(int i=0; i< zoom.size(); i++)
        {
            zoomLevel = zoom.at(i);
            dir.cd(zoomLevel);
            QStringList longitudes = dir.entryList(QDir::Dirs|QDir::NoDotAndDotDot);
            QString lon;
            for(int j=0; j< longitudes.size(); j++)
            {
                lon = longitudes.at(j);
                dir.cd(lon);
                QFileInfoList latitudes = dir.entryInfoList(QDir::Files|QDir::NoDotAndDotDot);
                QString lat;
                for(int k=0; k< latitudes.size(); k++)
                {
                    lat = latitudes.at(k).baseName();
                    cacheSize+= latitudes.at(k).size();
                    QString name = zoomLevel+"."+lon+"."+lat;
                    tileCache.insert(name,1);
                }
                dir.cdUp();//go back to zoom level folder
            }
            dir.cdUp();//go back to tile folder
        }
		QDir::setCurrent(folder);
		cout<<"cache size "<<(float)cacheSize/1024/1024<<" MB"<<endl;
	}
}

/**
Slot to keep track of download progress
*/
void cacaMap::slotDownloadProgress(qint64 _bytesReceived, qint64 _bytesTotal)
{
}

/**
* Slot that gets called everytime a %tile download request finishes
* Saves image file to HDD, takes out item from download queue, and  adds item to cache list
*/
void cacaMap::slotDownloadReady(QNetworkReply * _reply)
{
	QNetworkReply::NetworkError error = _reply->error();
	//get url of original request 
	QNetworkRequest req = _reply->request();
	QUrl url = req.url();
	QString surl = url.toString();
	bool found = false;
	QHash<QString,tile>::const_iterator i;
	i = downloadQueue.constBegin();
	
	for( i; i!=downloadQueue.constEnd();i++)
	{
		if (i.value().url == surl)
		{
			found = true;
			break;
		}
	}

	if (error == QNetworkReply::NoError)
	{
		qint64 bytes = _reply->bytesAvailable();

		if (bytes)
		{
			//get url of original request 
			QNetworkRequest req = _reply->request();
			QUrl url = req.url();
			QString surl = url.toString();
			cacheSize+=bytes;
			//get image data
			QByteArray data = _reply->readAll();		
			if (found)
			{
				tile nextItem = i.value();
				QString kk = i.key();
				QString zdir = QString().setNum(nextItem.zoom);
				QString xdir = QString().setNum(nextItem.x);
				QString tilefile = servermgr.fileName(nextItem.y);

				QDir::setCurrent(folder);
				QDir dir;
				if (!dir.exists(servermgr.tileCacheFolder()))
				{
					dir.mkdir(servermgr.tileCacheFolder());
				}
				dir.cd(servermgr.tileCacheFolder());

				if(!dir.exists(zdir))
				{
					dir.mkdir(zdir);	
				}
				dir.cd(zdir);
				if(!dir.exists(xdir))
				{
					dir.mkdir(xdir);	
				}
				dir.cd(xdir);
				
				QDir::setCurrent(dir.path());
				QFile f(tilefile);
				f.open(QIODevice::WriteOnly);
				quint64 byteswritten = f.write(data);
				if (byteswritten <= 0)
				{
					cout<<"error writing to file "<<f.fileName().toStdString()<<endl;
				}
				f.close();
				//remove item from download queue
				downloadQueue.remove(i.key());
				
				//add it to cache
				tileCache.insert(kk,1);
				//update with new tile
				updateBuffer();
				update();
			}
			else
			{
				cout<<"downloaded tile "<<surl.toStdString()<<" was not in Download queue. Data ignored"<<endl;
			}
			downloading = false;
			downloadPicture();
			
		}
		else
		{
			//cout<<"no data"<<endl;
		}
		update();
	}
	else
	{
        qDebug() <<"network error: ("<<error<<") "<<_reply->errorString();
		
		if(found)
		{
			//if content is not available we dont want to keep requesting it
			if (error == QNetworkReply::ContentNotFoundError)
			{
				unavailableTiles.insert(i.key(),1);
			}
			//remove the item from queue and try again
			downloadQueue.remove(i.key());
			downloading = false;
			downloadPicture();
		}
	}
	_reply->deleteLater();
}
/**
Slot that gets called when theres is an network error
*/
void cacaMap::slotError(QNetworkReply::NetworkError _code)
{
	cout<<"some error "<<_code<<endl;
}
/**
Widget resize event handler
*/
void cacaMap::resizeEvent(QResizeEvent* event)
{
	delete imgBuffer;
	imgBuffer = new QPixmap(size());
	updateContent();
}

/**
* Blits buffer to widget
*/
void cacaMap::renderMap(QPainter &p)
{
	//QRect dest(QPoint(0,0), size());
	if (buffzoomrate<1.0)
	{
		int ox = width()*(1-buffzoomrate)/2;
		int oy = height()*(1-buffzoomrate)/2;
		QRect src(QPoint(ox,oy),size()*buffzoomrate); 
		
		tmpbuff= imgBuffer->copy(src).scaled(size(),Qt::KeepAspectRatio);

		p.drawPixmap(0,0,tmpbuff);
		//p.drawPixmap(dest, *imgBuffer,src);//this is shorter but slower
	}
	else
	{
		p.drawPixmap(0,0,*imgBuffer);
	}
}
/**
Paint even handler
*/
void cacaMap::paintEvent(QPaintEvent *event)
{
	QPainter p(this);
	renderMap(p);
}

/**
destructor
*/
cacaMap::~cacaMap()
{
	delete manager;
	delete imgBuffer;
}
/**
Figures out which tiles are visible
Based on the current size of the widget, the %tile size, current coordinates and zoom level
*/
void cacaMap::updateTilesToRender()
{
	longPoint pixelCoords = myMercator::geoCoordToPixel(geocoords,zoom,tileSize); 

	//central tile coords
	qint32 xtile = pixelCoords.x/tileSize;
	qint32 ytile = pixelCoords.y/tileSize;
	//offset of central tile respect to the center of the widget
	int offsetx = pixelCoords.x % tileSize;
	int offsety = pixelCoords.y % tileSize;

	//num columns of tiles that fit left of the central tile
	float tilesleft = (float)(this->width()/2 - offsetx)/tileSize;
	
	//how many pixels overflow from the leftmost  tiles
	//second %tileSize is to take into account negative tilesLeft
	int globaloffsetx = (tileSize - (this->width()/2 - offsetx) % tileSize)%tileSize;

	//num rows of tiles that fit above the central tile
	float tilesup = (float)(this->height()/2 - offsety)/tileSize;

	//how many pixels overflow from top tiles
	int globaloffsety = (tileSize - (this->height()/2 - offsety) % tileSize)%tileSize;

	//num columns of tiles that fit right of central tile
	float tilesright = (float)(this->width()/2 + offsetx - tileSize)/tileSize;
	//num rows of tiles that fit under central tile
	float tilesbottom = (float)(this->height()/2 + offsety - tileSize)/tileSize;

	tilesToRender.left = xtile - ceil(tilesleft);
	tilesToRender.right = xtile + ceil(tilesright);
	tilesToRender.top =ytile - ceil(tilesup);
	tilesToRender.bottom = ytile + ceil(tilesbottom);
	tilesToRender.offsetx = globaloffsetx;
	tilesToRender.offsety = globaloffsety;
	tilesToRender.zoom = zoom;
}
/**
* Blits visible tiles buffer
*/
void cacaMap::updateBuffer()
{
	QPainter p(imgBuffer);
	imgBuffer->fill(Qt::gray);
	for (qint32 i= tilesToRender.left;i<= tilesToRender.right; i++)
	{
		for (qint32 j=tilesToRender.top ; j<= tilesToRender.bottom; j++)
		{
			QString x;
			QString y;
			//wrap around the tiles horizontally if i is outside [0,2^zoom]
			qint32 numtiles = 1<<tilesToRender.zoom;
			qint32 valx =((i<0)*numtiles + i%numtiles)%numtiles;
			x.setNum(valx);
			QPixmap image;
            int posx = (i-tilesToRender.left)*tileSize - tilesToRender.offsetx;
			int posy =  (j-tilesToRender.top)*tileSize - tilesToRender.offsety;
			//dont try to render tiles with y coords outside range
			//cause we cant do vertical wrapping!
			if (j>=0 && j<numtiles)
			{
				QString tileid = QString().setNum(tilesToRender.zoom) +"."+x+"."+QString().setNum(j);
				if (tileCache.contains(tileid))
				{
					//render the tile
					QDir::setCurrent(folder);
					//check path format (windows?)
					QString path= getTilePath(tilesToRender.zoom,valx) ;
					QString fileName = servermgr.fileName(j);
					QDir::setCurrent(path);
					QFile f(fileName);
					if (f.open(QIODevice::ReadOnly))
					{
						image.loadFromData(f.readAll());
						f.close();
					}
					else
					{
						cout<<"no file found "<<path.toStdString()<<fileName.toStdString()<<endl;
					}
				}
				//check if it's in the list of unavailable tiles
				else if (unavailableTiles.contains(tileid))
				{
					image = notAvailableTile;
				}
				//the tile is not cached so download it
                else if (enable_downloading)
				{
					//check that the image hasnt been queued already
					if (!downloadQueue.contains(tileid))
					{
						tile t;
						t.zoom = tilesToRender.zoom;
						t.x = valx;
						t.y = j;
						t.url = servermgr.getTileUrl(tilesToRender.zoom,valx,j);
						//queue the image for download
						downloadQueue.insert(tileid,t);
					}
					//crop a tile from a lower zoom level and use it as a patch(a la google maps)
					//while the tile is downloading	
                    image = getTilePatch(tilesToRender.zoom,valx,j,0,0,tileSize);
				}
				p.drawPixmap(posx,posy,image);
			}
		}
	}
	p.drawRect(0,0,width()-1, height()-1);
    if (enable_downloading && !downloading)
	{
		downloadPicture();
	}
}
/**
* calls the following two functions
* @see cacaMap::updateTilesToRender
* @see cacaMap::updateBuffer
*/
void cacaMap::updateContent()
{
	updateTilesToRender();
	updateBuffer();
}

cacaMapMouse::cacaMapMouse(QPointF startcoords,
                           bool enable_download,
                           QWidget* parent):cacaMap(startcoords,enable_download,parent)
{
    cout<<"derived constructor"<<endl;
    timer = new QTimer(this);
    mindistance = 0.025f;
    animrate = 0.5f;

    hlayout = new QHBoxLayout;

    slider = new QSlider(Qt::Vertical,this);
    slider->setTickPosition(QSlider::TicksBothSides);
    slider->setMaximum(maxZoom);
    slider->setMinimum(minZoom);
    slider->setSliderPosition(zoom);
    connect(slider, SIGNAL(valueChanged(int)),this, SLOT(updateZoom(int)));

    hlayout->addWidget(slider);
    hlayout->addStretch();
    setLayout(hlayout);

}

cacaMapMouse::~cacaMapMouse()
{
    delete slider;
    delete hlayout;
}

/**
Saves the screen coordinates of the last click
This is used for scrolling the map
@see myDerived::mouseMoveEvent()
*/
void cacaMapMouse::mousePressEvent(QMouseEvent* e)
{
    mouseAnchor = e->pos();
}

/**
Calculates the length of the mouse drag and
translates it into a new coordinate, map is rerendered
*/
void cacaMapMouse::mouseMoveEvent(QMouseEvent* e)
{
    QPoint delta = e->pos()- mouseAnchor;
    mouseAnchor = e->pos();
    longPoint p = myMercator::geoCoordToPixel(geocoords,zoom,tileSize);

    p.x-= delta.x();
    p.y-= delta.y();
    geocoords = myMercator::pixelToGeoCoord(p,zoom,tileSize);
    updateContent();
    update();
}

void cacaMapMouse::mouseDoubleClickEvent(QMouseEvent* e)
{
    //do the zoom-in animation magic
    if (e->button() == Qt::LeftButton)
    {
        QPoint deltapx = e->pos() - QPoint(width()/2,height()/2);
        longPoint currpospx = myMercator::geoCoordToPixel(geocoords,zoom,tileSize);
        longPoint newpospx;
        newpospx.x = currpospx.x + deltapx.x();
        newpospx.y = currpospx.y + deltapx.y();
        destination = myMercator::pixelToGeoCoord(newpospx,zoom,tileSize);
        connect(timer,SIGNAL(timeout()),this,SLOT(zoomAnim()));
        timer->start(40);
    }
    //do a simple zoom out for now
    else if (e->button() == Qt::RightButton)
    {
        zoomOut();
        slider->setSliderPosition(zoom);
        update();
    }
}

void cacaMapMouse::zoomAnim()
{
    float delta = buffzoomrate - 0.5;
    if (delta > mindistance)
    {
        QPointF deltaSpace = destination - geocoords;
        geocoords+=animrate*deltaSpace;
        buffzoomrate-= delta*animrate;
        updateContent();
    }
    //you are already there
    else
    {
        timer->stop();
        disconnect(timer,SIGNAL(timeout()),this,SLOT(zoomAnim()));
        geocoords = destination;
        buffzoomrate = 1.0;
        zoomIn();
        slider->setSliderPosition(zoom);
    }
    update();
}
void cacaMapMouse::updateZoom(int newZoom)
{
    setZoom(newZoom);
    update();
}
void cacaMapMouse::paintEvent(QPaintEvent *e)
{
    cacaMap::paintEvent(e);
    QPainter painter(this);
    painter.fillRect(QRect(0,0,width(),height()), QBrush(QColor(128, 128, 128, 128)));
}
