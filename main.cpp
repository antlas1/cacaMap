#include <qapplication.h>
#include "cacamap.h"

int main (int argc, char **argv)
{
	QApplication a(argc, argv);
    QPointF start_coord( 30.3141,59.9386);
    cacaMapMouse myWidget(start_coord,true);
	myWidget.show();
	return a.exec();
}
