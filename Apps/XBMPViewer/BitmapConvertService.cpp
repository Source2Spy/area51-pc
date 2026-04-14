//==============================================================================
//
//  BitmapConvertService.cpp
//
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#include "BitmapConvertService.h"

#include "ConvertUtils.h"
#include "FileListModel.h"
#include "aux_Bitmap.hpp"

#include <QFileInfo>

//==============================================================================
//  IMPLEMENTATION
//==============================================================================

BitmapConvertService::ContextNeeds
BitmapConvertService::GetContextNeeds(const QVector<const FileRecord*>& Records) const
{
    ContextNeeds Needs;
    Needs.NeedTga = FALSE;
    Needs.NeedXbmp = FALSE;

    for (s32 i = 0; i < Records.size(); ++i)
    {
        const FileRecord* pRecord = Records[i];
        if (!pRecord)
            continue;

        const QString Ext = QFileInfo(pRecord->Path).suffix().toLower();
        if (Ext == "xbmp" || Ext == "xbm")
            Needs.NeedTga = TRUE;
        else if (Ext == "psd" || Ext == "tga" || Ext == "bmp")
            Needs.NeedXbmp = TRUE;
    }

    return Needs;
}

//==============================================================================

void BitmapConvertService::ConvertToTga(const QVector<const FileRecord*>& Records,
                                        const QString& OutputDir) const
{
    for (s32 i = 0; i < Records.size(); ++i)
    {
        const FileRecord* pRecord = Records[i];
        if (!pRecord)
            continue;

        const QString OutFile = OutputDir + "/" + QFileInfo(pRecord->Name).completeBaseName() + ".tga";
        const QByteArray PathBytes = pRecord->Path.toLocal8Bit();
        const QByteArray OutBytes = OutFile.toLocal8Bit();

        xbitmap Bitmap;
        if (!auxbmp_Load(Bitmap, PathBytes.constData()))
        {
            x_DebugMsg("xbmpViewer: failed to load bitmap: %s\n", PathBytes.constData());
            continue;
        }

        if (Bitmap.GetFlags() & xbitmap::FLAG_GCN_DATA_SWIZZLED)
            Bitmap.GCNUnswizzleData();

        switch (Bitmap.GetFormat())
        {
        case xbitmap::FMT_DXT1:
        case xbitmap::FMT_DXT3:
        case xbitmap::FMT_DXT5:
            auxbmp_Decompress(Bitmap);
            break;
        default:
            break;
        }

        Bitmap.SaveTGA(OutBytes.constData());
    }
}

//==============================================================================

void BitmapConvertService::ConvertToXbmp(const QVector<const FileRecord*>& Records,
                                         const QString& OutputDir,
                                         const QString& Platform,
                                         xbitmap::format TargetFormat,
                                         s32 MipLevels,
                                         const QString& SelectedGameType) const
{
    for (s32 i = 0; i < Records.size(); ++i)
    {
        const FileRecord* pRecord = Records[i];
        if (!pRecord)
            continue;

        const QString OutFile = OutputDir + "/" + QFileInfo(pRecord->Name).completeBaseName() + ".xbmp";
        const QByteArray PathBytes = pRecord->Path.toLocal8Bit();
        const QByteArray OutBytes = OutFile.toLocal8Bit();

        xbitmap Bitmap;
        if (!auxbmp_Load(Bitmap, PathBytes.constData()))
        {
            x_DebugMsg("xbmpViewer: failed to load bitmap: %s\n", PathBytes.constData());
            continue;
        }

        if (Platform == "PC" || Platform == "Xbox")
            auxbmp_ConvertToD3D(Bitmap);
        else if (Platform == "PS2")
            auxbmp_ConvertToPS2(Bitmap);
        else if (Platform == "GameCube")
            auxbmp_ConvertToGCN(Bitmap);
        else if (Platform == "Native")
            auxbmp_ConvertToNative(Bitmap);
        else
        {
            x_DebugMsg("xbmpViewer: platform selection error: %s\n", Platform.toLocal8Bit().constData());
            continue;
        }

        Bitmap.ConvertFormat(TargetFormat);

        if (MipLevels > 0 && MipLevels <= 16)
        {
            const s32 Width = Bitmap.GetWidth();
            const s32 Height = Bitmap.GetHeight();
            if (IsPowerOfTwo(Width) && IsPowerOfTwo(Height))
                Bitmap.BuildMips(MipLevels);
            else
                x_DebugMsg("xbmpViewer: mip generation requires power-of-two dimensions: %s\n",
                           PathBytes.constData());
        }

        if (!Bitmap.Save(OutBytes.constData()))
            x_DebugMsg("xbmpViewer: failed to save xbmp: %s\n", OutBytes.constData());
        else if (SelectedGameType == "The Hobbit")
            PatchHobbitFormatInFile(OutFile, RemapFormatForHobbit(TargetFormat));
    }
}

//==============================================================================

void BitmapConvertService::ConvertContext(const QVector<const FileRecord*>& Records,
                                          const QString& OutputDir,
                                          const QString& Platform,
                                          xbitmap::format TargetFormat,
                                          s32 MipLevels,
                                          const QString& SelectedGameType) const
{
    QVector<const FileRecord*> XbmpRecords;
    QVector<const FileRecord*> SourceRecords;

    for (s32 i = 0; i < Records.size(); ++i)
    {
        const FileRecord* pRecord = Records[i];
        if (!pRecord)
            continue;

        const QString Ext = QFileInfo(pRecord->Path).suffix().toLower();
        if (Ext == "xbmp" || Ext == "xbm")
            XbmpRecords.push_back(pRecord);
        else if (Ext == "psd" || Ext == "tga" || Ext == "bmp")
            SourceRecords.push_back(pRecord);
    }

    ConvertToTga(XbmpRecords, OutputDir);
    ConvertToXbmp(SourceRecords, OutputDir, Platform, TargetFormat, MipLevels, SelectedGameType);
}
