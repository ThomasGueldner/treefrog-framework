/* Copyright (c) 2016, AOYAMA Kazuharu
 * All rights reserved.
 *
 * This software may be used and distributed according to the terms of
 * the New BSD License, which is incorporated herein by reference.
 */

#include <QJSEngine>
#include <QJSValue>
#include <TJSLoader>
#include <TJSContext>
#include <TJSInstance>
#include "tsystemglobal.h"

//#define tSystemError(fmt, ...)  printf(fmt "\n", ## __VA_ARGS__)
//#define tSystemDebug(fmt, ...)  printf(fmt "\n", ## __VA_ARGS__)

static QStringList searchPaths = { "." };


inline const char *prop(const QJSValue &val, const QString &name = QString())
{
    return (name.isEmpty()) ? qPrintable(val.toString()) : qPrintable(val.property(name).toString());
}


TJSContext::TJSContext(QObject *parent)
    : QObject(parent), jsEngine(new QJSEngine()), loadedFiles(), funcObj(nullptr),
      lastFunc(), mutex(QMutex::Recursive)
{
    jsEngine->evaluate("exports={};module={};module.exports={};");
}


TJSContext::~TJSContext()
{
    if (funcObj) {
        delete funcObj;
    }
    delete jsEngine;
}


QJSValue TJSContext::evaluate(const QString &program, const QString &fileName, int lineNumber)
{
    QMutexLocker locker(&mutex);

    QJSValue ret = jsEngine->evaluate(program, fileName, lineNumber);
    if (ret.isError()) {
        tSystemError("JS uncaught exception at %s:%s : %s", prop(ret, "fileName"),
                     prop(ret, "lineNumber"), prop(ret, "message"));
    }
    return ret;
}


QJSValue TJSContext::call(const QString &func, const QJSValue &arg)
{
    QJSValueList args = { arg };
    return call(func, args);
}


QJSValue TJSContext::call(const QString &func, const QJSValueList &args)
{
    QMutexLocker locker(&mutex);
    QJSValue ret;

    QString funcsym = QString::number(args.count()) + func;
    if (funcsym != lastFunc || !funcObj) {
        lastFunc = funcsym;

        QString argstr;
        for (int i = 0; i < args.count(); i++) {
            argstr = QChar('a') + QString::number(i) + ',';
        }
        argstr.chop(1);

        QString defFunc = QString("function(%1){return(%2(%1));}").arg(argstr, func);

        if (!funcObj) {
            funcObj = new QJSValue();
        }

        *funcObj = evaluate(defFunc);
        if (funcObj->isError()) {
            goto eval_error;
        }
    }

    ret = funcObj->call(args);
    if (ret.isError()) {
        tSystemError("JS uncaught exception at %s:%s : %s", prop(ret, "fileName"),
                     prop(ret, "lineNumber"), prop(ret));
        goto eval_error;
    }

    return ret;

eval_error:
    delete funcObj;
    funcObj = nullptr;
    return ret;
}


TJSInstance TJSContext::callAsConstructor(const QString &constructorName, const QJSValue &arg)
{
    QJSValueList args = { arg };
    return callAsConstructor(constructorName, args);
}


TJSInstance TJSContext::callAsConstructor(const QString &constructorName, const QJSValueList &args)
{
    QMutexLocker locker(&mutex);

    QJSValue construct = evaluate(constructorName);
    tSystemDebug("construct: %s", qPrintable(construct.toString()));
    QJSValue res = construct.callAsConstructor(args);
    if (res.isError()) {
        tSystemError("JS uncaught exception at %s:%s : %s", prop(res, "fileName"),
                     prop(res, "lineNumber"), prop(res));
    }
    return TJSInstance(res);
}


QJSValue TJSContext::import(const QString &moduleName)
{
    TJSLoader loader(moduleName);
    return loader.importTo(this, false);
}


QJSValue TJSContext::import(const QString &defaultMember, const QString &moduleName)
{
    TJSLoader loader(defaultMember, moduleName);
    return loader.importTo(this, false);
}