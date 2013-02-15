/*
 * YeastTableModel.cpp is part of Brewtarget, and is Copyright Philip G. Lee
 * (rocketman768@gmail.com), 2009-2013.
 *
 * Brewtarget is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * Brewtarget is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <QAbstractTableModel>
#include <QAbstractItemModel>
#include <QWidget>
#include <QModelIndex>
#include <QVariant>
#include <QItemDelegate>
#include <QStyleOptionViewItem>
#include <QComboBox>
#include <QLineEdit>
#include <QString>

#include <QVector>
#include "database.h"
#include "yeast.h"
#include "YeastTableModel.h"
#include "unit.h"
#include "brewtarget.h"
#include "recipe.h"

YeastTableModel::YeastTableModel(QTableView* parent, bool editable)
: QAbstractTableModel(parent), editable(editable), parentTableWidget(parent), recObs(0)
{
   yeastObs.clear();
}

void YeastTableModel::addYeast(Yeast* yeast)
{
   if( yeastObs.contains(yeast) )
      return;
   // If we are observing the database, ensure that the item is undeleted and
   // fit to display.
   if(
      recObs == 0 &&
      (
         yeast->deleted() ||
         !yeast->display()
      )
   )
      return;
   int size = yeastObs.size();
   beginInsertRows( QModelIndex(), size, size );
   yeastObs.append(yeast);
   connect( yeast, SIGNAL(changed(QMetaProperty,QVariant)), this, SLOT(changed(QMetaProperty,QVariant)) );
   //reset(); // Tell everybody that the table has changed.
   endInsertRows();
   
   if(parentTableWidget)
   {
      parentTableWidget->resizeColumnsToContents();
      parentTableWidget->resizeRowsToContents();
   }
}

void YeastTableModel::observeRecipe(Recipe* rec)
{
   if( recObs )
   {
      disconnect( recObs, 0, this, 0 );
      removeAll();
   }
   
   recObs = rec;
   if( recObs )
   {
      connect( recObs, SIGNAL(changed(QMetaProperty,QVariant)), this, SLOT(changed(QMetaProperty,QVariant)) );
      addYeasts( recObs->yeasts() );
   }
}

void YeastTableModel::observeDatabase(bool val)
{
   if( val )
   {
      observeRecipe(0);
      
      removeAll();
      connect( &(Database::instance()), SIGNAL(newYeastSignal(Yeast*)), this, SLOT(addYeast(Yeast*)) );
      connect( &(Database::instance()), SIGNAL(deletedYeastSignal(Yeast*)), this, SLOT(removeYeast(Yeast*)) );
      addYeasts( Database::instance().yeasts() );
   }
   else
   {
      removeAll();
      disconnect( &(Database::instance()), 0, this, 0 );
   }
}

void YeastTableModel::addYeasts(QList<Yeast*> yeasts)
{
   QList<Yeast*>::iterator i;
   QList<Yeast*> tmp;
   
   for( i = yeasts.begin(); i != yeasts.end(); i++ )
   {
      if( !yeastObs.contains(*i) )
         tmp.append(*i);
   }
   
   int size = yeastObs.size();
   if (size+tmp.size())
   {
      beginInsertRows( QModelIndex(), size, size+tmp.size()-1 );
      yeastObs.append(tmp);
      
      for( i = tmp.begin(); i != tmp.end(); i++ )
         connect( *i, SIGNAL(changed(QMetaProperty,QVariant)), this, SLOT(changed(QMetaProperty,QVariant)) );
      
      endInsertRows();
   }

   if( parentTableWidget )
   {
      parentTableWidget->resizeColumnsToContents();
      parentTableWidget->resizeRowsToContents();
   }
   
}

void YeastTableModel::removeYeast(Yeast* yeast)
{
   int i = yeastObs.indexOf(yeast);

   if( i >= 0 )
   {
      beginRemoveRows( QModelIndex(), i, i );
      disconnect( yeast, 0, this, 0 );
      yeastObs.removeAt(i);
      //reset(); // Tell everybody the table has changed.
      endRemoveRows();
      
      if(parentTableWidget)
      {
         parentTableWidget->resizeColumnsToContents();
         parentTableWidget->resizeRowsToContents();
      }
   }
}

void YeastTableModel::removeAll()
{
   if (yeastObs.size())
   {
      beginRemoveRows( QModelIndex(), 0, yeastObs.size()-1 );
      while( !yeastObs.isEmpty() )
      {
         disconnect( yeastObs.takeLast(), 0, this, 0 );
      }
      endRemoveRows();
   }
}

void YeastTableModel::changed(QMetaProperty prop, QVariant /*val*/)
{
   int i;

   // Find the notifier in the list
   Yeast* yeastSender = qobject_cast<Yeast*>(sender());
   if( yeastSender )
   {
      i = yeastObs.indexOf(yeastSender);
      if( i < 0 )
         return;
      
      emit dataChanged( QAbstractItemModel::createIndex(i, 0),
                        QAbstractItemModel::createIndex(i, YEASTNUMCOLS-1));
      return;
   }
   
   // See if sender is our recipe.
   Recipe* recSender = qobject_cast<Recipe*>(sender());
   if( recSender && recSender == recObs )
   {
      if( QString(prop.name()) == "yeasts" )
      {
         removeAll();
         addYeasts( recObs->yeasts() );
      }
      if( rowCount() > 0 )
         emit headerDataChanged( Qt::Vertical, 0, rowCount()-1 );
      return;
   }
}

int YeastTableModel::rowCount(const QModelIndex& /*parent*/) const
{
   return yeastObs.size();
}

int YeastTableModel::columnCount(const QModelIndex& /*parent*/) const
{
   return YEASTNUMCOLS;
}

QVariant YeastTableModel::data( const QModelIndex& index, int role ) const
{
   Yeast* row;
   unitDisplay unit;

   // Ensure the row is ok.
   if( index.row() >= (int)yeastObs.size() )
   {
      Brewtarget::log(Brewtarget::WARNING, tr("Bad model index. row = %1").arg(index.row()));
      return QVariant();
   }
   else
      row = yeastObs[index.row()];

   switch( index.column() )
   {
      case YEASTNAMECOL:
      if( role == Qt::DisplayRole )
         return QVariant(row->name());
      else
         return QVariant();
      case YEASTTYPECOL:
      if( role == Qt::DisplayRole )
         return QVariant(row->typeStringTr());
      else if( role == Qt::UserRole )
         return QVariant(row->type());
      else
         return QVariant();
      case YEASTLABCOL:
      if( role == Qt::DisplayRole )
         return QVariant(row->laboratory());
      else
         return QVariant();
      case YEASTPRODIDCOL:
      if( role == Qt::DisplayRole )
         return QVariant(row->productID());
      else
         return QVariant();
      case YEASTFORMCOL:
      if( role == Qt::DisplayRole )
         return QVariant(row->formStringTr());
      else if( role == Qt::UserRole )
         return QVariant(row->form());
      else
         return QVariant();
      case YEASTAMOUNTCOL:
      if( role != Qt::DisplayRole )
         return QVariant();

      unit  = displayUnit(index.column());

      return QVariant( 
                        Brewtarget::displayAmount( row->amount(), 
                                                   row->amountIsWeight() ? (Unit*)Units::kilograms : (Unit*)Units::liters, 
                                                   3, 
                                                   unit, 
                                                   noScale 
                                                 ) 
                     );

      default :
         Brewtarget::log(Brewtarget::WARNING, tr("Bad column: %1").arg(index.column()));
         return QVariant();
   }
}

QVariant YeastTableModel::headerData( int section, Qt::Orientation orientation, int role ) const
{
   if( orientation == Qt::Horizontal && role == Qt::DisplayRole )
   {
      switch( section )
      {
         case YEASTNAMECOL:
            return QVariant(tr("Name"));
         case YEASTTYPECOL:
            return QVariant(tr("Type"));
         case YEASTFORMCOL:
            return QVariant(tr("Form"));
         case YEASTAMOUNTCOL:
            return QVariant(tr("Amount"));
         case YEASTLABCOL:
             return QVariant(tr("Laboratory"));
         case YEASTPRODIDCOL:
             return QVariant(tr("Product ID"));
         default:
            Brewtarget::log(Brewtarget::WARNING, tr("Bad column: %1").arg(section));
            return QVariant();
      }
   }
   else
      return QVariant();
}

Qt::ItemFlags YeastTableModel::flags(const QModelIndex& index ) const
{
   int col = index.column();
   switch(col)
   {
      case YEASTNAMECOL:
         return Qt::ItemIsSelectable | Qt::ItemIsDragEnabled | Qt::ItemIsEnabled;
      default:
         return Qt::ItemIsSelectable | (editable ? Qt::ItemIsEditable : Qt::NoItemFlags) | Qt::ItemIsDragEnabled |
            Qt::ItemIsEnabled;
   }
}

bool YeastTableModel::setData( const QModelIndex& index, const QVariant& value, int role )
{
   Yeast *row;
   unitDisplay unit;

   if( index.row() >= (int)yeastObs.size() || role != Qt::EditRole )
      return false;
   else
      row = yeastObs[index.row()];

   switch( index.column() )
   {
      case YEASTNAMECOL:
         if( value.canConvert(QVariant::String))
         {
            row->setName(value.toString());
            return true;
         }
         else
            return false;
      case YEASTLABCOL:
         if( value.canConvert(QVariant::String) )
         {
            row->setLaboratory(value.toString());
            return true;
         }
         else
            return false;
      case YEASTPRODIDCOL:
         if( value.canConvert(QVariant::String) )
         {
            row->setProductID(value.toString());
            return true;
         }
         else
            return false;
      case YEASTTYPECOL:
         if( value.canConvert(QVariant::Int) )
         {
            row->setType(static_cast<Yeast::Type>(value.toInt()));
            return true;
         }
         else
            return false;
      case YEASTFORMCOL:
         if( value.canConvert(QVariant::Int) )
         {
            row->setForm(static_cast<Yeast::Form>(value.toInt()));
            return true;
         }
         else
            return false;
      case YEASTAMOUNTCOL:
         if( value.canConvert(QVariant::String) )
         {
            unit = displayUnit(YEASTAMOUNTCOL);
            row->setAmount( row->amountIsWeight() ? 
                            Brewtarget::weightQStringToSI(value.toString(),unit) : 
                            Brewtarget::volQStringToSI(value.toString(),unit) );
            return true;
         }
         else
            return false;
      default:
         Brewtarget::log(Brewtarget::WARNING, tr("Bad column: %1").arg(index.column()));
         return false;
   }
}

Yeast* YeastTableModel::getYeast(unsigned int i)
{
   return yeastObs[i];
}

unitDisplay YeastTableModel::displayUnit(int column) const
{ 
   QString attribute = generateName(column);

   if ( attribute.isEmpty() )
      return noUnit;

   return (unitDisplay)Brewtarget::option(attribute, QVariant(-1), this, Brewtarget::UNIT).toInt();
}

unitScale YeastTableModel::displayScale(int column) const
{ 
   QString attribute = generateName(column);

   if ( attribute.isEmpty() )
      return noScale;

   return (unitScale)Brewtarget::option(attribute, QVariant(-1), this, Brewtarget::SCALE).toInt();
}

// We need to:
//   o clear the custom scale if set
//   o clear any custom unit from the rows
//      o which should have the side effect of clearing any scale
void YeastTableModel::setDisplayUnit(int column, unitDisplay displayUnit) 
{
   // Yeast* row; // disabled per-cell magic
   QString attribute = generateName(column);

   if ( attribute.isEmpty() )
      return;

   Brewtarget::setOption(attribute,displayUnit,this,Brewtarget::UNIT); 
   Brewtarget::setOption(attribute,noScale,this,Brewtarget::SCALE);

   /* Disabled cell-specific code
   for (int i = 0; i < rowCount(); ++i )
   {
      row = getYeast(i);
      row->setDisplayUnit(noUnit);
   }
   */
}

// Setting the scale should clear any cell-level scaling options
void YeastTableModel::setDisplayScale(int column, unitScale displayScale) 
{ 
   // Yeast* row; //disabled per-cell magic

   QString attribute = generateName(column);

   if ( attribute.isEmpty() )
      return;

   Brewtarget::setOption(attribute,displayScale,this,Brewtarget::SCALE); 

   /* disabled cell-specific code
   for (int i = 0; i < rowCount(); ++i )
   {
      row = getYeast(i);
      row->setDisplayScale(noScale);
   }
   */
}

QString YeastTableModel::generateName(int column) const
{
   QString attribute;

   switch(column)
   {
      case YEASTAMOUNTCOL:
         attribute = "amount";
         break;
      default:
         attribute = "";
   }
   return attribute;
}

//==========================CLASS YeastItemDelegate===============================

YeastItemDelegate::YeastItemDelegate(QObject* parent)
        : QItemDelegate(parent)
{
}

QWidget* YeastItemDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem& /*option*/, const QModelIndex &index) const
{
   int col = index.column();

   if( col == YEASTTYPECOL )
   {
      QComboBox *box = new QComboBox(parent);

      box->addItem(tr("Ale"));
      box->addItem(tr("Lager"));
      box->addItem(tr("Wheat"));
      box->addItem(tr("Wine"));
      box->addItem(tr("Champagne"));
      box->setSizeAdjustPolicy(QComboBox::AdjustToContents);

      return box;
   }
   else if( col == YEASTFORMCOL )
   {
      QComboBox *box = new QComboBox(parent);

      box->addItem(tr("Liquid"));
      box->addItem(tr("Dry"));
      box->addItem(tr("Slant"));
      box->addItem(tr("Culture"));

      return box;
   }
   else
      return new QLineEdit(parent);
}

void YeastItemDelegate::setEditorData(QWidget *editor, const QModelIndex &index) const
{
   int col = index.column();

   if( col == YEASTTYPECOL || col == YEASTFORMCOL )
   {
      QComboBox* box = qobject_cast<QComboBox*>(editor);
      int ndx = index.model()->data(index, Qt::UserRole).toInt();

      box->setCurrentIndex(ndx);
   }
   else
   {
      QLineEdit* line = qobject_cast<QLineEdit*>(editor);

      line->setText(index.model()->data(index, Qt::DisplayRole).toString());
   }

}

void YeastItemDelegate::setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const
{
   int col = index.column();

   if( col == YEASTTYPECOL || col == YEASTFORMCOL )
   {
      QComboBox* box = (QComboBox*)editor;
      int value = box->currentIndex();

      model->setData(index, value, Qt::EditRole);
   }
   else
   {
      QLineEdit* line = (QLineEdit*)editor;

      model->setData(index, line->text(), Qt::EditRole);
   }
}

void YeastItemDelegate::updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex& /*index*/) const
{
   editor->setGeometry(option.rect);
}
