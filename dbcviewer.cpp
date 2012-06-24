#include "dbcviewer.h"
#include "ui_dbcviewer.h"

DBCViewer::DBCViewer(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::DBCViewer),
    lastFile("./"),
    fileInfo(QApplication::applicationFilePath())
{
    ui->setupUi(this);
    dbcDialog.setLabelText(QFileDialog::LookIn, "Please select DBC file");
    dbcDialog.setDirectory(lastFile);
    dbcDialog.setNameFilter("DBC File (*.dbc)");
    dbcDialog.selectNameFilter("DBC File (*.dbc)");

    db2Dialog.setLabelText(QFileDialog::LookIn, "Please select DB2 file");
    db2Dialog.setDirectory(lastFile);
    db2Dialog.setNameFilter("DB2 File (*.db2)");
    db2Dialog.selectNameFilter("DB2 File (*.db2)");

    saveDialog.setLabelText(QFileDialog::LookIn, "Please select file where to save SQL");
    saveDialog.setAcceptMode(QFileDialog::AcceptSave);
    saveDialog.setDirectory(lastFile);
    saveDialog.setNameFilter("SQL File (*.sql)");
    saveDialog.selectNameFilter("SQL File (*.sql)");
}

DBCViewer::~DBCViewer()
{
    delete ui;
}

void DBCViewer::on_actionQuit_triggered()
{
    QApplication::quit();
}

void DBCViewer::LoadDBCIntoTable(QString file, QString format)
{
    if (loader.isOpen())
        loader.close();
    loader.setFileName(file);
    if (!loader.open(QIODevice::ReadOnly))
    {
        QMessageBox message;
        message.setText("Cannot open file " + file);
        message.exec();
        ui->statusBar->showMessage("Cannot open file " + file);
        return;
    }

    char title[5];
    int records;
    int fields;
    int recordSize;
    int stringsSize;

    QDataStream stream(&loader);
    stream.setByteOrder(QDataStream::LittleEndian);
    stream.readRawData(title, 4);
    if (qstrncmp(title, "WDBC", 4) != 0)
    {
        QMessageBox message;
        message.setText("This it not DBC File !!");
        message.exec();
        ui->statusBar->showMessage("File is not DBC " + file);
        return;
    }
    lastFile = file;
    fileInfo = QFileInfo(file);
    stream >> records;
    stream >> fields;
    stream >> recordSize;
    stream >> stringsSize;
    printf("File: '%s'  Records: %d  fields: %d  recordSize: %d  stringsSize: %d\n", file.toAscii().constData(), records, fields, recordSize, stringsSize);
    for (int i = ui->tableWidget->columnCount(); i < fields; ++i)
        ui->tableWidget->insertColumn(ui->tableWidget->columnCount());

    for (int i = ui->tableWidget->rowCount(); i < records; ++i)
        ui->tableWidget->insertRow(ui->tableWidget->rowCount());

    for (int i = ui->tableWidget->columnCount(); i > fields; --i)
        ui->tableWidget->removeColumn(ui->tableWidget->columnCount() - 1);

    for (int i = ui->tableWidget->rowCount(); i > records; --i)
        ui->tableWidget->removeRow(ui->tableWidget->rowCount() - 1);

    ui->searchColumn->clear();
    ui->searchColumn->insertItem(ui->searchColumn->count(), "Id");
    for (int i = ui->searchColumn->count(); i < ui->tableWidget->columnCount(); ++i)
        ui->searchColumn->insertItem(ui->searchColumn->count(), "Column " + QString::number(i + 1));

    int* table = new int[records * recordSize];

    for (int i = 0; i < records; ++i)
    {
        stream.readRawData((char*)(table + i * recordSize), recordSize);
    }
    quint64 loadBegin = QDateTime::currentMSecsSinceEpoch();
    char* strings = new char[stringsSize];
    stream.readRawData(strings, stringsSize);
    printf("Loaded in: %llu ms\n", QDateTime::currentMSecsSinceEpoch() - loadBegin);
    ui->tableWidget->setUpdatesEnabled(false);


    if (format.length() == fields)
        fieldTypes = format;
    else
    {
        loadBegin = QDateTime::currentMSecsSinceEpoch();
        fieldTypes= QString(fields, 's');
        fieldTypes[0] = 'n';

        for (register int i = 0; i < 50; ++i)
        {
            for (register int b = 1; b < fields; ++b)
            {
                if (table[i * recordSize + b] > 0 &&
                    table[i * recordSize + b] < stringsSize  &&
                    *(strings + table[i * recordSize + b] - 1) == 0 &&
                    *(strings + table[i * recordSize + b]) >= 'A' &&
                    *(strings + table[i * recordSize + b]) <= 'z' &&
                    fieldTypes[b] == 's')
                    fieldTypes[b] = 's';
                else
                {
                    if (*(float*)(table + i * recordSize + b) - int(*(float*)(table + i * recordSize + b)) != 0 && log10(table[i * recordSize + b]) > 6)
                        fieldTypes[b] = 'f';
                    else
                        fieldTypes[b] = 'i';
                }
            }
        }
        printf("Automatic Detection finished in: %llu ms\n", QDateTime::currentMSecsSinceEpoch() - loadBegin);
    }

    loadBegin = QDateTime::currentMSecsSinceEpoch();
    for (register int i = 0; i < records; ++i)
    {
        for (register int b = 0; b < fields; ++b)
        {
            QTableWidgetItem* item = ui->tableWidget->item(i, b);
            bool created = false;
            if (!item)
            {
                item = new QTableWidgetItem();
                created = true;
            }
            switch (fieldTypes[b].toAscii())
            {
                case 's': // strings;
                    item->setText(QString(strings + table[i * recordSize + b]));
                    break;
                case 'n': // index
                case 'i': // ints
                    item->setData(Qt::DisplayRole, table[i * recordSize + b]);
                    break;
                case 'f': // float
                    item->setData(Qt::DisplayRole, *(float*)(table + i * recordSize + b));
                    break;
                case 'x': //
                    break;
            }
            if (created)
                ui->tableWidget->setItem(i, b, item);
        }
    }
    printf("Inserting data into table finished in: %llu ms\n", QDateTime::currentMSecsSinceEpoch() - loadBegin);
    ui->tableWidget->setUpdatesEnabled(true);
    ui->statusBar->showMessage(QString("Loaded ")+QString::number(records)+" rows");
    loader.close();
    delete [] table;
    delete [] strings;
}

void DBCViewer::LoadDB2IntoTable(QString file, QString format)
{
    if (loader.isOpen())
        loader.close();
    loader.setFileName(file);
    if (!loader.open(QIODevice::ReadOnly))
    {
        QMessageBox message;
        message.setText("Cannot open file " + file);
        message.exec();
        ui->statusBar->showMessage("Cannot open file " + file);
        return;
    }

    char title[5];
    int records;
    int fields;
    int recordSize;
    int stringsSize;
    int tableHash;
    int build;
    int unk;
    int maxIndex;
    int locales;
    int unk2;



    QDataStream stream(&loader);
    stream.setByteOrder(QDataStream::LittleEndian);
    stream.readRawData(title, 4);
    if (qstrncmp(title, "WDB2", 4) != 0)
    {
        QMessageBox message;
        message.setText("This it not DB2 File !!");
        message.exec();
        ui->statusBar->showMessage("File is not DB2 " + file);
        return;
    }
    lastFile = file;
    fileInfo = QFileInfo(file);
    stream >> records;
    stream >> fields;
    stream >> recordSize;
    stream >> stringsSize;
    stream >> tableHash;
    stream >> build;
    stream >> unk;

    if (build > 12880)
    {
        stream >> unk2;
        stream >> maxIndex;
        stream >> locales;
        stream >> unk2;
    }

    printf("File: '%s'  Records: %d  fields: %d  recordSize: %d  stringsSize: %d tableHash: %d build %d\n", file.toAscii().constData(), records, fields, recordSize, stringsSize, tableHash, build);
    for (int i = ui->tableWidget->columnCount(); i < fields; ++i)
        ui->tableWidget->insertColumn(ui->tableWidget->columnCount());

    for (int i = ui->tableWidget->rowCount(); i < records; ++i)
        ui->tableWidget->insertRow(ui->tableWidget->rowCount());

    for (int i = ui->tableWidget->columnCount(); i > fields; --i)
        ui->tableWidget->removeColumn(ui->tableWidget->columnCount() - 1);

    for (int i = ui->tableWidget->rowCount(); i > records; --i)
        ui->tableWidget->removeRow(ui->tableWidget->rowCount() - 1);

    ui->searchColumn->clear();
    ui->searchColumn->insertItem(ui->searchColumn->count(), "Id");
    for (int i = ui->searchColumn->count(); i < ui->tableWidget->columnCount(); ++i)
        ui->searchColumn->insertItem(ui->searchColumn->count(), "Column " + QString::number(i + 1));

    if (maxIndex != 0)
    {
        int diff = maxIndex - unk2 + 1;
        stream.skipRawData(diff * 6);
    }

    int* table = new int[records * recordSize];

    for (int i = 0; i < records; ++i)
    {
        stream.readRawData((char*)(table + i * recordSize), recordSize);
    }
    quint64 loadBegin = QDateTime::currentMSecsSinceEpoch();
    char* strings = new char[stringsSize];
    stream.readRawData(strings, stringsSize);
    printf("Loaded in: %llu ms\n", QDateTime::currentMSecsSinceEpoch() - loadBegin);
    ui->tableWidget->setUpdatesEnabled(false);


    if (format.length() == fields)
        fieldTypes = format;
    else
    {
        loadBegin = QDateTime::currentMSecsSinceEpoch();
        fieldTypes= QString(fields, 's');
        fieldTypes[0] = 'n';

        for (register int i = 0; i < 50; ++i)
        {
            for (register int b = 1; b < fields; ++b)
            {
                if (table[i * recordSize + b] > 0 &&
                    table[i * recordSize + b] < stringsSize  &&
                    *(strings + table[i * recordSize + b] - 1) == 0 &&
                    *(strings + table[i * recordSize + b]) >= 'A' &&
                    *(strings + table[i * recordSize + b]) <= 'z' &&
                    fieldTypes[b] == 's')
                    fieldTypes[b] = 's';
                else
                {
                    if (*(float*)(table + i * recordSize + b) - int(*(float*)(table + i * recordSize + b)) != 0 && log10(table[i * recordSize + b]) > 6)
                        fieldTypes[b] = 'f';
                    else
                        fieldTypes[b] = 'i';
                }
            }
        }
        printf("Automatic Detection finished in: %llu ms\n", QDateTime::currentMSecsSinceEpoch() - loadBegin);
    }

    loadBegin = QDateTime::currentMSecsSinceEpoch();
    for (register int i = 0; i < records; ++i)
    {
        for (register int b = 0; b < fields; ++b)
        {
            QTableWidgetItem* item = ui->tableWidget->item(i, b);
            bool created = false;
            if (!item)
            {
                item = new QTableWidgetItem();
                created = true;
            }
            switch (fieldTypes[b].toAscii())
            {
                case 's': // strings;
                    item->setText(QString(strings + table[i * recordSize + b]));
                    break;
                case 'n': // index
                case 'i': // ints
                    item->setData(Qt::DisplayRole, table[i * recordSize + b]);
                    break;
                case 'f': // float
                    item->setData(Qt::DisplayRole, *(float*)(table + i * recordSize + b));
                    break;
                case 'x': //
                    break;
            }
            if (created)
                ui->tableWidget->setItem(i, b, item);
        }
    }
    printf("Inserting data into table finished in: %llu ms\n", QDateTime::currentMSecsSinceEpoch() - loadBegin);
    ui->tableWidget->setUpdatesEnabled(true);
    ui->statusBar->showMessage(QString("Loaded ")+QString::number(records)+" rows");
    loader.close();
    delete [] table;
    delete [] strings;
}

void DBCViewer::ReloadDBCIntoTable()
{
    if (!lastFile.size())
    {
        QMessageBox message;
        message.setText("You have not opened any file sucessfully..");
        message.exec();
        ui->statusBar->showMessage("Cannot open file " + lastFile);
        return;
    }
    if (!loader.isOpen())
        loader.close();
    if (!loader.open(QIODevice::ReadOnly))
    {
        QMessageBox message;
        message.setText("Cannot open file " + lastFile);
        message.exec();
        ui->statusBar->showMessage("Cannot open file " + lastFile);
        return;
    }

    char title[5];
    int records;
    int fields;
    int recordSize;
    int stringsSize;

    QDataStream stream(&loader);
    stream.setByteOrder(QDataStream::LittleEndian);
    stream.readRawData(title, 4);
    if (qstrncmp(title, "WDBC", 4) != 0)
    {
        QMessageBox message;
        message.setText("This it not DBC File !!");
        message.exec();
        ui->statusBar->showMessage("File is not DBC " + lastFile);
        return;
    }
    stream >> records;
    stream >> fields;
    stream >> recordSize;
    stream >> stringsSize;

    int* table = new int[records * recordSize];

    for (int i = 0; i < records; ++i)
    {
        stream.readRawData((char*)(table + i * recordSize), recordSize);
    }

    char* strings = new char[stringsSize];
    stream.readRawData(strings, stringsSize);

    ui->tableWidget->setUpdatesEnabled(false);

    for (register int i = 0; i < records; ++i)
    {
        for (register int b = 0; b < fields; ++b)
        {
            QTableWidgetItem* item = ui->tableWidget->item(i, b);
            bool created = false;
            if (!item)
            {
                item = new QTableWidgetItem();
                created = true;
            }
            switch (fieldTypes[b].toAscii())
            {
                case 's': // strings;
                    item->setText(QString(strings + table[i * recordSize + b]));
                    break;
                case 'n': // index
                case 'i': // ints
                    item->setData(Qt::DisplayRole, table[i * recordSize + b]);
                    break;
                case 'f': // float
                    item->setData(Qt::DisplayRole, *(float*)(table + i * recordSize + b));
                    break;
                case 'x': //
                    break;
            }
            if (created)
                ui->tableWidget->setItem(i, b, item);
        }
    }
    ui->tableWidget->setUpdatesEnabled(true);
    ui->statusBar->showMessage("File Loaded");
    loader.close();
    delete [] table;
    delete [] strings;
}

void DBCViewer::ReloadDB2IntoTable()
{
    if (!lastFile.size())
    {
        QMessageBox message;
        message.setText("You have not opened any file sucessfully..");
        message.exec();
        ui->statusBar->showMessage("Cannot open file " + lastFile);
        return;
    }
    if (!loader.isOpen())
        loader.close();
    if (!loader.open(QIODevice::ReadOnly))
    {
        QMessageBox message;
        message.setText("Cannot open file " + lastFile);
        message.exec();
        ui->statusBar->showMessage("Cannot open file " + lastFile);
        return;
    }
    char title[5];
    int records;
    int fields;
    int recordSize;
    int stringsSize;
    int tableHash;
    int build;
    int unk;
    int maxIndex;
    int locales;
    int unk2;



    QDataStream stream(&loader);
    stream.setByteOrder(QDataStream::LittleEndian);
    stream.readRawData(title, 4);
    stream >> records;
    stream >> fields;
    stream >> recordSize;
    stream >> stringsSize;
    stream >> tableHash;
    stream >> build;
    stream >> unk;

    if (build > 12880)
    {
        stream >> unk2;
        stream >> maxIndex;
        stream >> locales;
        stream >> unk2;
    }

    if (maxIndex != 0)
    {
        int diff = maxIndex - unk2 + 1;
        stream.skipRawData(diff * 6);
    }

    int* table = new int[records * recordSize];

    for (int i = 0; i < records; ++i)
    {
        stream.readRawData((char*)(table + i * recordSize), recordSize);
    }

    char* strings = new char[stringsSize];
    stream.readRawData(strings, stringsSize);

    ui->tableWidget->setUpdatesEnabled(false);

    for (register int i = 0; i < records; ++i)
    {
        for (register int b = 0; b < fields; ++b)
        {
            QTableWidgetItem* item = ui->tableWidget->item(i, b);
            bool created = false;
            if (!item)
            {
                item = new QTableWidgetItem();
                created = true;
            }
            switch (fieldTypes[b].toAscii())
            {
                case 's': // strings;
                    item->setText(QString(strings + table[i * recordSize + b]));
                    break;
                case 'n': // index
                case 'i': // ints
                    item->setData(Qt::DisplayRole, table[i * recordSize + b]);
                    break;
                case 'f': // float
                    item->setData(Qt::DisplayRole, *(float*)(table + i * recordSize + b));
                    break;
                case 'x': //
                    break;
            }
            if (created)
                ui->tableWidget->setItem(i, b, item);
        }
    }
    ui->tableWidget->setUpdatesEnabled(true);
    ui->statusBar->showMessage("File Loaded");
    loader.close();
    delete [] table;
    delete [] strings;
}


void DBCViewer::on_tableWidget_currentCellChanged(int currentRow, int /* currentColumn */, int previousRow, int /* previousColumn */)
{
    for (int i = 0; i < ui->tableWidget->columnCount(); ++i)
        if (QTableWidgetItem* item = ui->tableWidget->item(previousRow, i))
            item->setBackground(QBrush(Qt::transparent));
    for (int i = 0; i < ui->tableWidget->columnCount(); ++i)
        if (QTableWidgetItem* item = ui->tableWidget->item(currentRow, i))
            item->setBackground(QBrush(QColor(0xFF, 0x6E, 0x00)));
}

void DBCViewer::on_actionGet_triggered()
{
    QMessageBox msg(QMessageBox::Information, "Get Format String", "Actual format string is: '" + fieldTypes + "'", QMessageBox::Ok);
    msg.exec();
}

void DBCViewer::on_actionSet_triggered()
{
    QString input = QInputDialog::getText(this, "Set Format String", "Enter Format String :");
    if (input.size() != fieldTypes.size())
    {
        QMessageBox msg(QMessageBox::Information, "Invalid Format string", "Format string you have entered does not match length of actual format string", QMessageBox::Ok);
        msg.exec();
    }
    else
        fieldTypes = input;
}

void DBCViewer::on_actionReload_triggered()
{
    if (!lastFile.length())
        return;
    if (lastFile.endsWith(".dbc"))
        ReloadDBCIntoTable();
    if (lastFile.endsWith(".db2"))
        ReloadDB2IntoTable();
}

void DBCViewer::on_searchButton_clicked()
{
    if (!ui->searchContent->text().trimmed().size())
        return;
    for (int i = ui->tableWidget->currentRow() + 1; i < ui->tableWidget->rowCount(); ++i)
        if (QTableWidgetItem* item = ui->tableWidget->item(i, ui->searchColumn->currentIndex()))
            if (item->text() == ui->searchContent->text())
            {
                ui->tableWidget->setCurrentItem(item);
                return;
            }
    if (ui->tableWidget->currentRow())
    {
        QMessageBox msg;
        msg.setText("Searched value was not found in column " + QString::number(ui->searchColumn->currentIndex()) + ".\nContinue from begining ?");
        msg.addButton(QMessageBox::Ok);
        msg.addButton(QMessageBox::Cancel);
        int result = msg.exec();
        if (result == QMessageBox::Ok)
        {
            int continueUntil = ui->tableWidget->currentRow() + 1;
            for (int i = 0; i < continueUntil; ++i)
                if (QTableWidgetItem* item = ui->tableWidget->item(i, ui->searchColumn->currentIndex()))
                    if (item->text() == ui->searchContent->text())
                    {
                        ui->tableWidget->setCurrentItem(item);
                        return;
                    }
        }
        else
            return;
    }
    QMessageBox::information(this, "Not found", "Searched value not found");
}

void DBCViewer::on_actionAutomatic_Field_Detection_triggered()
{
    if (!dbcDialog.exec())
        return;
    QString file = dbcDialog.selectedFiles().first();
    if (!file.trimmed().size())
        return;
    ui->statusBar->showMessage("Loading... " + file);
    LoadDBCIntoTable(file, "");
}

void DBCViewer::on_actionManual_Field_Setup_triggered()
{
    if (!dbcDialog.exec())
        return;
    QString file = dbcDialog.selectedFiles().first();
    if (!file.trimmed().size())
        return;
    QString input = QInputDialog::getText(this, "Set Format String", "Enter Format String :");
    ui->statusBar->showMessage("Loading... " + file);
    LoadDBCIntoTable(file, input);
}

void DBCViewer::on_actionExport_To_SQL_triggered()
{
    if (!saveDialog.exec())
        return;
    QString exportFileName = saveDialog.selectedFiles().first();
    QString tableName = QInputDialog::getText(this, "Set Table name", "Enter table name :");
    QFile exportFile;
    exportFile.setFileName(exportFileName);
    if (!exportFile.open(QIODevice::WriteOnly))
    {
        QMessageBox message;
        message.setText("Cannot open file");
        message.exec();
        ui->statusBar->showMessage("Cannot open file " + exportFileName);
        return;
    }
    QTextStream stream(&exportFile);
    stream << "INSERT INTO ";
    stream << "`" << tableName <<"` ";
    stream << "VALUES\n";
    for (int i = 0; i < this->ui->tableWidget->rowCount(); ++i)
    {
        stream << '(';
        for (int b = 0; b < this->ui->tableWidget->columnCount(); ++b)
        {
            stream << "'" << this->ui->tableWidget->item(i, b)->text() << "'";
            if (b != this->ui->tableWidget->columnCount() - 1)
                stream << ',';
        }
        stream << ')';
        if (i != this->ui->tableWidget->rowCount() - 1)
            stream << ",\n";
    }
    stream << ';';
    exportFile.close();

}

void DBCViewer::on_actionAutomatic_field_detection_triggered()
{
    if (!db2Dialog.exec())
        return;
    QString file = db2Dialog.selectedFiles().first();
    if (!file.trimmed().size())
        return;
    ui->statusBar->showMessage("Loading... " + file);
    LoadDB2IntoTable(file, "");
}

void DBCViewer::on_actionManual_field_Setup_triggered()
{
    if (!db2Dialog.exec())
        return;
    QString file = db2Dialog.selectedFiles().first();
    if (!file.trimmed().size())
        return;
    QString input = QInputDialog::getText(this, "Set Format String", "Enter Format String :");
    ui->statusBar->showMessage("Loading... " + file);
    LoadDB2IntoTable(file, input);
}
