#include <QDebug>
#include <QFont>
#include "utils.h"
#include "key.h"

key::key(QString t,QObject *parent) : QObject(parent)
{
  text = t;
  X =10;
  Y =10;
  W =82;//t.length()*4 + 78;
  H =62;
  pressed = false;
  repetionActive = false;

  setObjectName(t);
}

QRect key::getRect()
{
    return QRect(X,Y,W,H);
}

void key::setX(int x )
{
  //  qDebug() << "setX " << x << " " << text;
  X = x;
}


void key::setY(int y )
{
  //  qDebug() << "setY " << y;
  Y = y;
}


void key::setIconFile(QString i )
{
  iconFilename = i;
}

void key::setPressed( bool b)
{
//   qDebug() << "setPessed " << b;
   pressed = b;
}

void key::draw(QPainter *p,QStyle *style)
{
    QStyleOptionButton opt;
    QFont font;
    auto fontAwesome = ModifyFont( font, "FontAwesome", 20.0 );

    opt.palette = QPalette(QColor(0,51,102,127));
    p->setFont(fontAwesome);

    /*
    if ( pressed )
           {
               opt.state = QStyle::State_Active;
           } else {
                opt.state = QStyle::State_Enabled;
           }
           */
    opt.rect = QRect(X, Y, W, H);

    if ( iconFilename !="" )
    {
        opt.icon     = QIcon(iconFilename);
        opt.iconSize = QSize(32,32);
    }
    else
    {
        if (text =="&") opt.text = "&&";
        else            opt.text = text;
    }
    style->drawControl(QStyle::CE_PushButton, &opt, p);
}
