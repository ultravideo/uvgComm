#include "sipauthentication.h"

#include "sipfieldcomposinghelper.h"
#include "sipconversions.h"

#include "common.h"

#include <QVariant>

#include <QRandomGenerator>
#include <QCryptographicHash>

SIPAuthentication::SIPAuthentication()
{}


void SIPAuthentication::processOutgoingRequest(SIPRequest& request,
                                               QVariant& content)
{
  printNormal(this, "Processing outgoing request");

  if (request.method == SIP_REGISTER)
  {
    for (auto& challenge : wwwChallenges_)
    {
      authorizations_.push_back(generateAuthResponse(challenge,
                                                  request.message->from.address.uri.userinfo.user,
                                                  request.requestURI, request.method, content
                                                                    )
                                               );
    }

    wwwChallenges_.clear(); // this makes sure we don't generate same responses again

    request.message->authorization = authorizations_;

  }
  else
  {
    for (auto& challenge : proxyChallenges_)
    {
      proxyAuthorizations_.push_back(generateAuthResponse(challenge,
                                                       request.message->from.address.uri.userinfo.user,
                                                       request.requestURI, request.method, content
                                                                         )
                                                    );
    }

    proxyChallenges_.clear();

    request.message->proxyAuthorization = proxyAuthorizations_;
  }

  emit outgoingRequest(request, content);
}


void SIPAuthentication::processIncomingResponse(SIPResponse& response,
                                                QVariant& content)
{
  printNormal(this, "Processing incoming response");

  // I think this is the REGISTER authorization
  if (response.type == SIP_UNAUTHORIZED)
  {
    wwwChallenges_ = response.message->wwwAuthenticate;
    authorizations_.clear();
  }
  else if (response.type == SIP_PROXY_AUTHENTICATION_REQUIRED)
  {
    proxyChallenges_ = response.message->proxyAuthenticate;
    proxyAuthorizations_.clear();
  }

  emit incomingResponse(response, content);
}


DigestResponse SIPAuthentication::generateAuthResponse(DigestChallenge& challenge,
                                                       QString username,
                                                       SIP_URI& requestURI,
                                                       SIPRequestMethod method,
                                                       QVariant& content)
{
  // This function should follow RFC 2617 (or subsequent updates)

  // TODO: check changes in RFC 7235

  // Latin1 is the ISO/IEC 8859-1. In other words an 8-bit representation
  // of hashed characters in a string.

  // TODO: when should new cnonce be generated?

  // TODO: what is the relation of MD5-sess and gop-options?

  DigestResponse response;

  // always present values
  response.username = username;
  response.realm = challenge.realm;
  response.nonce = challenge.nonce;

  // these are copied if they existed in challenge
  response.opaque = challenge.opaque;
  response.algorithm = challenge.algorithm;

  // digest
  response.digestUri = std::shared_ptr<SIP_URI> (new SIP_URI(requestURI));

  // TODO: Handle wrong password

  // TODO: Stale tells us that we should retry with new nonce

  QCryptographicHash hash(QCryptographicHash::Md5);

  // A1 is only calculated the first time if algorithm is MD5-sess
  if (a1_.isEmpty() && challenge.algorithm == SIP_MD5_SESS)
  {
    // get saved hash from
    QString qopString = settingString("local/Credentials");

    if (qopString == "")
    {
      printProgramError(this, "Did not find credentials when authenthicating");
      return {};
    }

    // generate client nonce (cnonce)
    QRandomGenerator* generator = QRandomGenerator::system();
    response.cnonce = QString::number(generator->generate(), 16);
    hash.addData(QString(qopString.toLatin1() + ":" + challenge.nonce + ":" + response.cnonce).toLatin1());

    a1_ = hash.result().toHex();
    hash.reset();

  }
  else if (challenge.algorithm == SIP_NO_ALGORITHM ||
           challenge.algorithm == SIP_MD5)
  {
    QString qopString = settingString("local/Credentials");
    a1_ = qopString.toLatin1();
  }
  else if (challenge.algorithm == SIP_UNKNOWN_ALGORITHM)
  {
    printWarning(this, "Unknown digest algorithm found! Not generating response");
    return {};
  }

  QString digestURI = composeSIPURI(requestURI);
  QString method_str = requestMethodToString(method);

  hash.addData(QString(method_str + ":" + digestURI).toLatin1());


  if (challenge.qopOptions.contains(SIP_AUTH_INT))
  {
    QCryptographicHash contentHash(QCryptographicHash::Md5);
    contentHash.addData(content.toByteArray());

    hash.addData(QString(":" + contentHash.result().toHex()).toLatin1());
  }

  QByteArray ha2 = hash.result().toHex();
  hash.reset();

  QString dResponse = "";

  if (challenge.qopOptions.contains(SIP_AUTH_INT) ||
      challenge.qopOptions.contains(SIP_AUTH))
  {
    // sets the nonceCount
    updateNonceCount(challenge, response);

    QString auth = challenge.qopOptions.contains(SIP_AUTH_INT) ? "auth-int" : "auth";
    dResponse = a1_ + ":" + challenge.nonce + ":" + response.nonceCount +
        ":" + response.cnonce + ":" + auth + ":" + ha2;

    response.messageQop = challenge.qopOptions.contains(SIP_AUTH_INT)
        ? SIP_AUTH_INT : SIP_AUTH;
  }
  else
  {
    dResponse = a1_ + ":" + challenge.nonce + ":" + ha2;
    response.messageQop = SIP_NO_AUTH;
  }

  hash.addData(dResponse.toLatin1());
  response.dresponse = hash.result().toHex();

  printDebug(DEBUG_NORMAL, this, "Calculated auth challenge response",
             {"ha1", "ha2"}, {QString(a1_), QString(ha2)});

  return response;
}


void SIPAuthentication::updateNonceCount(DigestChallenge& challenge,
                                         DigestResponse& response)
{
  if (realmToNonce_.find(challenge.realm) != realmToNonce_.end())
  {
    if (realmToNonce_[challenge.realm] != challenge.nonce)
    {
      realmToNonce_[challenge.realm] = challenge.nonce;
      realmToNonceCount_[challenge.realm] = 0;
    }
  }
  else
  {
    realmToNonce_[challenge.realm] = challenge.nonce;
    realmToNonceCount_[challenge.realm] = 0;
  }

  ++realmToNonceCount_[challenge.realm];

  QString nCountStr  = QString::number(realmToNonceCount_[challenge.realm], 8);

  response.nonceCount = nCountStr.rightJustified(8, '0');
}
