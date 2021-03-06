/*
 * simplesasl.cpp - Simple SASL implementation
 * Copyright (C) 2003  Justin Karneges
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include "simplesasl.h"

#include <qca.h>
#include <stdlib.h>
#include "base64.h"

namespace XMPP
{

struct Prop
{
	QByteArray var, val;
};

class PropList : public QList<Prop>
{
public:
	PropList() : QList<Prop>()
	{
	}

	void set(const QByteArray &var, const QByteArray &val)
	{
		Prop p;
		p.var = var;
		p.val = val;
		append(p);
	}

	QByteArray get(const QByteArray &var)
	{
		for(ConstIterator it = begin(); it != end(); ++it) {
			if((*it).var == var)
				return (*it).val;
		}
		return QByteArray();
	}

	QByteArray toString() const
	{
		QByteArray str;
		bool first = true;
		for(ConstIterator it = begin(); it != end(); ++it) {
			if(!first)
				str += ',';
			str += (*it).var + "=\"" + (*it).val + '\"';
			first = false;
		}
		return str;
	}

	bool fromString(const QByteArray &str)
	{
		PropList list;
		int at = 0;
		while(1) {
			int n = str.indexOf('=', at);
			if(n == -1)
				break;
			QByteArray var, val;
			var = str.mid(at, n-at);
			at = n + 1;
			if(str[at] == '\"') {
				++at;
				n = str.indexOf('\"', at);
				if(n == -1)
					break;
				val = str.mid(at, n-at);
				at = n + 1;
			}
			else {
				n = str.indexOf(',', at);
				if(n != -1) {
					val = str.mid(at, n-at);
					at = n;
				}
				else {
					val = str.mid(at);
					at = str.length()-1;
				}
			}
			Prop prop;
			prop.var = var;
			prop.val = val;
			list.append(prop);

			if(str[at] != ',')
				break;
			++at;
		}

		// integrity check
		if(list.varCount("nonce") != 1)
			return false;
		if(list.varCount("algorithm") != 1)
			return false;
		*this = list;
		return true;
	}

	int varCount(const QByteArray &var)
	{
		int n = 0;
		for(ConstIterator it = begin(); it != end(); ++it) {
			if((*it).var == var)
				++n;
		}
		return n;
	}

	QStringList getValues(const QByteArray &var)
	{
		QStringList list;
		for(ConstIterator it = begin(); it != end(); ++it) {
			if((*it).var == var)
				list += (*it).val;
		}
		return list;
	}
};

class SimpleSASLContext : public QCA_SASLContext
{
public:
	// core props
	QString service, host;

	// state
	int step;
	QByteArray in_buf;
	QString out_mech;
	QByteArray out_buf;
	bool capable;
	int err;

	QCA_SASLNeedParams need;
	QCA_SASLNeedParams have;
	QString user, authz, pass, realm;

	SimpleSASLContext()
	{
		reset();
	}

	~SimpleSASLContext()
	{
		reset();
	}

	void reset()
	{
		resetState();
		resetParams();
	}

	void resetState()
	{
		out_mech = QString();
		out_buf.resize(0);
		err = -1;
	}

	void resetParams()
	{
		capable = true;
		need.user = false;
		need.authzid = false;
		need.pass = false;
		need.realm = false;
		have.user = false;
		have.authzid = false;
		have.pass = false;
		have.realm = false;
		user = QString();
		authz = QString();
		pass = QString();
		realm = QString();
	}

	void setCoreProps(const QString &_service, const QString &_host, QCA_SASLHostPort *, QCA_SASLHostPort *)
	{
		service = _service;
		host = _host;
	}

	void setSecurityProps(bool, bool, bool, bool, bool reqForward, bool reqCreds, bool reqMutual, int ssfMin, int, const QString &, int)
	{
		if(reqForward || reqCreds || reqMutual || ssfMin > 0)
			capable = false;
		else
			capable = true;
	}

	int security() const
	{
		return 0;
	}

	int errorCond() const
	{
		return err;
	}

	bool clientStart(const QStringList &mechlist)
	{
		bool haveMech = false;
		for(QStringList::ConstIterator it = mechlist.begin(); it != mechlist.end(); ++it) {
			if((*it) == "DIGEST-MD5") {
				haveMech = true;
				break;
			}
		}
		if(!capable || !haveMech) {
			err = QCA::SASL::NoMech;
			return false;
		}

		resetState();
		step = 0;
		return true;
	}

	int clientFirstStep(bool)
	{
		return clientTryAgain();
	}

	bool serverStart(const QString &, QStringList *, const QString &)
	{
		return false;
	}

	int serverFirstStep(const QString &, const QByteArray *)
	{
		return Error;
	}

	QCA_SASLNeedParams clientParamsNeeded() const
	{
		return need;
	}

	void setClientParams(const QString *_user, const QString *_authzid, const QString *_pass, const QString *_realm)
	{
		if(_user) {
			user = *_user;
			need.user = false;
			have.user = true;
		}
		if(_authzid) {
			authz = *_authzid;
			need.authzid = false;
			have.authzid = true;
		}
		if(_pass) {
			pass = *_pass;
			need.pass = false;
			have.pass = true;
		}
		if(_realm) {
			realm = *_realm;
			need.realm = false;
			have.realm = true;
		}
	}

	QString username() const
	{
		return QString();
	}

	QString authzid() const
	{
		return QString();
	}

	int nextStep(const QByteArray &in)
	{
		in_buf = in;
		return tryAgain();
	}

	int tryAgain()
	{
		return clientTryAgain();
	}

	QString mech() const
	{
		return out_mech;
	}

	const QByteArray *clientInit() const
	{
		return 0;
	}

	QByteArray result() const
	{
		return out_buf;
	}

	int clientTryAgain()
	{
		if(step == 0) {
			out_mech = "DIGEST-MD5";
			++step;
			return Continue;
		}
		else if(step == 1) {
			// if we still need params, then the app has failed us!
			if(need.user || need.authzid || need.pass || need.realm) {
				err = -1;
				return Error;
			}

			// see if some params are needed
			if(!have.user)
				need.user = true;
			if(!have.authzid)
				need.authzid = true;
			if(!have.pass)
				need.pass = true;
			if(need.user || need.authzid || need.pass)
				return NeedParams;

			// get props
			QByteArray cs(in_buf.data(), in_buf.size());
			PropList in;
			if(!in.fromString(cs)) {
				err = QCA::SASL::BadProto;
				return Error;
			}

			// make a cnonce
			QByteArray a(32, 0);
			for(int n = 0; n < (int)a.size(); ++n)
				a[n] = (char)(256.0*rand()/(RAND_MAX+1.0));
			QByteArray cnonce = Base64::arrayToString(a).toLatin1();

			// make other variables
			realm = host;
			QByteArray nonce = in.get("nonce");
			QByteArray nc = "00000001";
			QByteArray uri = service.toUtf8() + '/' + host.toUtf8();
			QByteArray qop = "auth";

			// build 'response'
			QByteArray X = user.toUtf8() + ':' + realm.toUtf8() + ':' + pass.toUtf8();
			QByteArray Y = QCA::MD5::hash(X);
			QByteArray tmp = QByteArray(":") + nonce + ':' + cnonce + ':' + authz.toUtf8();
			QByteArray A1(Y.size() + tmp.length(), 0);
			memcpy(A1.data(), Y.data(), Y.size());
			memcpy(A1.data() + Y.size(), tmp.data(), tmp.length());
			QByteArray A2 = "AUTHENTICATE:" + uri;
			QByteArray HA1 = QCA::MD5::hashToString(A1).toLatin1();
			QByteArray HA2 = QCA::MD5::hashToString(A2).toLatin1();
			QByteArray KD = HA1 + ':' + nonce + ':' + nc + ':' + cnonce + ':' + qop + ':' + HA2;
			QByteArray Z = QCA::MD5::hashToString(KD).toLatin1();

			// build output
			PropList out;
			out.set("username", user.toUtf8());
			out.set("realm", host.toUtf8());
			out.set("nonce", nonce);
			out.set("cnonce", cnonce);
			out.set("nc", nc);
			out.set("serv-type", service.toUtf8());
			out.set("host", host.toUtf8());
			out.set("digest-uri", uri);
			out.set("qop", qop);
			out.set("response", Z);
			out.set("charset", "utf-8");
			out.set("authzid", authz.toUtf8());
			QByteArray s = out.toString();

			// done
			//out_buf.resize(s.length());
			//memcpy(out_buf.data(), s.data(), out_buf.size());
			out_buf = s;
			++step;
			return Continue;
		}
		else {
			out_buf.resize(0);
			return Success;
		}
	}

	bool encode(const QByteArray &a, QByteArray *b)
	{
		*b = a;
		return true;
	}

	bool decode(const QByteArray &a, QByteArray *b)
	{
		*b = a;
		return true;
	}
};

class QCASimpleSASL : public QCAProvider
{
public:
	QCASimpleSASL() {}
	~QCASimpleSASL() {}

	void init()
	{
	}

	int qcaVersion() const
	{
		return QCA_PLUGIN_VERSION;
	}

	int capabilities() const
	{
		return QCA::CAP_SASL;
	}

	void *context(int cap)
	{
		if(cap == QCA::CAP_SASL)
			return new SimpleSASLContext;
		return 0;
	}
};

QCAProvider *createProviderSimpleSASL()
{
	return (new QCASimpleSASL);
}

}
