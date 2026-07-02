// Module
// File: database_global.h   Version: 0.1.0   License: AGPLv3
// Created: HeZhiyuan      2026-06-13 13:14:55
// Description:定义Database动态库的导入与导出宏
//
#pragma once

#include <QtCore/QtGlobal>

#if defined(DATABASE_LIBRARY)
#  define DATABASE_EXPORT Q_DECL_EXPORT
#else
#  define DATABASE_EXPORT Q_DECL_IMPORT
#endif