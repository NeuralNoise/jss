/* 
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Netscape Security Services for Java.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 1998-2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s):
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 */
#include "_jni/org_mozilla_jss_CryptoManager.h"

#include <plarena.h>
#include <secmodt.h>
#include <pk11func.h>
#include <secerr.h>
#include <nspr.h>
#include <cert.h>
#include <certdb.h>
#include <key.h>
#include <secpkcs7.h>

#include <jssutil.h>

#include <jss_exceptions.h>
#include "pk11util.h"
#include <java_ids.h>


/*****************************************************************
 *
 * CryptoManager. f i n d C e r t B y N i c k n a m e N a t i v e
 *
 */
JNIEXPORT jobject JNICALL
Java_org_mozilla_jss_CryptoManager_findCertByNicknameNative
  (JNIEnv *env, jobject this, jstring nickname)
{
    char *nick=NULL;
    jobject certObject=NULL;
    CERTCertificate *cert=NULL;

    PR_ASSERT(env!=NULL && this!=NULL && nickname!=NULL);

    nick = (char*) (*env)->GetStringUTFChars(env, nickname, NULL);
    PR_ASSERT(nick!=NULL);

    cert = PK11_FindCertFromNickname(nick, NULL);

    if(cert == NULL) {
        cert = CERT_FindCertByNickname( CERT_GetDefaultCertDB(), nick );
        if( cert == NULL ) {
            JSS_nativeThrow(env, OBJECT_NOT_FOUND_EXCEPTION);
            goto finish;
        }
    }

    certObject = JSS_PK11_wrapCert(env, &cert);

finish:
    if(nick != NULL) {
        (*env)->ReleaseStringUTFChars(env, nickname, nick);
    }
    if(cert != NULL) {
        CERT_DestroyCertificate(cert);
    }
    return certObject;
}

/*
 * Creates or adds to a list of all certs with a give nickname, sorted by
 * validity time, newest first.  Invalid certs are considered older than valid
 * certs. If validOnly is set, do not include invalid certs on list.
 */
static CERTCertList *
CreateNicknameCertList(CERTCertList *certList, CERTCertDBHandle *handle,
                            char *nickname, int64 sorttime, PRBool validOnly)
{
    CERTCertificate *cert;
    CERTCertList *ret;

    cert = CERT_FindCertByNickname(handle, nickname);
    if ( cert == NULL ) {
        cert = PK11_FindCertFromNickname(nickname,NULL);
        if( cert == NULL ) {
            return NULL;
        }
    }

    ret = CERT_CreateSubjectCertList(certList, handle, &cert->derSubject,
                                     sorttime, validOnly);

    CERT_DestroyCertificate(cert);

    return(ret);
}

/*****************************************************************
 *
 * CryptoManager. f i n d C e r t s B y N i c k n a m e N a t i v e
 *
 */
JNIEXPORT jobjectArray JNICALL
Java_org_mozilla_jss_CryptoManager_findCertsByNicknameNative
  (JNIEnv *env, jobject this, jstring nickname)
{
    CERTCertList *list =NULL;
    jobjectArray certArray=NULL;
    CERTCertListNode *node;
    const char *nickChars=NULL;
    jboolean charsAreCopied;
    jclass certClass;
    int count;
    int i;

    /* convert the nickname string */
    nickChars = (*env)->GetStringUTFChars(env, nickname, &charsAreCopied);
    if( nickChars == NULL ) {
        goto finish;
    }

    /* get the list of certs with the given nickname */
    list = CreateNicknameCertList(NULL, CERT_GetDefaultCertDB(),
                    (char*)nickChars, PR_Now(), PR_FALSE /* validOnly */);
    if( list == NULL ) {
        count = 0;
    } else {
        /* Since this structure changed in NSS_2_7_RTM (the reference */
        /* to "count" was removed from the "list" structure) we must  */
        /* now count up the number of nodes manually!                 */
        for( node = CERT_LIST_HEAD(list), count=0;
             ! CERT_LIST_END(node, list);
             node = CERT_LIST_NEXT(node), count++ );
    }
    PR_ASSERT(count >= 0);

    /* create the cert array */
    certClass = (*env)->FindClass(env, X509_CERT_CLASS);
    if( certClass == NULL ) {
        goto finish;
    }
    certArray = (*env)->NewObjectArray(env, count, certClass, NULL);
    if( certArray == NULL ) {
        /* exception was thrown */
        goto finish;
    }

    if( list == NULL ) {
        goto finish;
    }

    /* traverse the list, placing each cert into the array */
    for(    node = CERT_LIST_HEAD(list), i=0;
            ! CERT_LIST_END(node, list);
            node = CERT_LIST_NEXT(node), i++       )     {

        CERTCertificate *cert;
        jobject certObj;

        /* Create a Java certificate object from the current CERTCertificate */
        cert = CERT_DupCertificate(node->cert);
        certObj = JSS_PK11_wrapCert(env, &cert);
        if( certObj == NULL ) {
            goto finish;
        }

        /* put the Java certificate into the next element in the array */
        (*env)->SetObjectArrayElement(env, certArray, i, certObj);

        if( (*env)->ExceptionOccurred(env) ) {
            goto finish;
        }
    }

    /* sanity check */
    PR_ASSERT( i == count );

finish:
    if(list) {
        CERT_DestroyCertList(list);
    }
    if( nickChars && charsAreCopied ) {
        (*env)->ReleaseStringUTFChars(env, nickname, nickChars);
    }
    return certArray;
}

/*****************************************************************
 *
 * CryptoManager.findCertByIssuerAndSerialNumberNative
 *
 */
JNIEXPORT jobject JNICALL
Java_org_mozilla_jss_CryptoManager_findCertByIssuerAndSerialNumberNative
  (JNIEnv *env, jobject this, jbyteArray issuerBA, jbyteArray serialNumBA)
{
    jobject certObject=NULL;
    CERTCertificate *cert=NULL;
    SECItem *issuer=NULL, *serialNum=NULL;
    CERTIssuerAndSN issuerAndSN;
    PK11SlotInfo *slot=NULL;

    PR_ASSERT(env!=NULL && this!=NULL);

    /* validate args */
    if( issuerBA == NULL || serialNumBA == NULL ) {
        JSS_throwMsg(env, ILLEGAL_ARGUMENT_EXCEPTION,
            "NULL parameter passed to CryptoManager.findCertByIssuer"
            "AndSerialNumberNative");
        goto finish;
    }

    /* convert byte arrays to SECItems */
    issuer = JSS_ByteArrayToSECItem(env, issuerBA);
    if( issuer == NULL ) {
        goto finish; }
    serialNum = JSS_ByteArrayToSECItem(env, serialNumBA);
    if( serialNum == NULL ) {
        goto finish; }
    issuerAndSN.derIssuer = *issuer;
    issuerAndSN.serialNumber = *serialNum;

    /* lookup with PKCS #11 first, then use cert database */
    cert = PK11_FindCertByIssuerAndSN(&slot, &issuerAndSN, NULL /*wincx*/);
    if( cert == NULL ) {
        cert = CERT_FindCertByIssuerAndSN(
                        CERT_GetDefaultCertDB(),
                        &issuerAndSN);
        if( cert == NULL ) {
            JSS_nativeThrow(env, OBJECT_NOT_FOUND_EXCEPTION);
            goto finish;
        }
    }

    certObject = JSS_PK11_wrapCert(env, &cert);

finish:
    if(slot) {
        PK11_FreeSlot(slot);
    }
    if(cert != NULL) {
        CERT_DestroyCertificate(cert);
    }
    if(issuer) {
        SECITEM_FreeItem(issuer, PR_TRUE /*freeit*/);
    }
    if(serialNum) {
        SECITEM_FreeItem(serialNum, PR_TRUE /*freeit*/);
    }
    return certObject;
}

/*****************************************************************
 *
 * CryptoManager. f i n d P r i v K e y B y C e r t N a t i v e
 *
 */
JNIEXPORT jobject JNICALL
Java_org_mozilla_jss_CryptoManager_findPrivKeyByCertNative
  (JNIEnv *env, jobject this, jobject Cert)
{
    PRThread *pThread;
    CERTCertificate *cert;
    SECKEYPrivateKey *privKey=NULL;
    jobject Key;

    pThread = PR_AttachThread(PR_SYSTEM_THREAD, 0, NULL);
    PR_ASSERT( pThread != NULL);
    PR_ASSERT( env!=NULL && this!=NULL && Cert!=NULL);

    if( JSS_PK11_getCertPtr(env, Cert, &cert) != PR_SUCCESS) {
        PR_ASSERT( (*env)->ExceptionOccurred(env) != NULL);
        goto finish;
    }
    if(cert==NULL) {
        PR_ASSERT(PR_FALSE);
        JSS_throw(env, OBJECT_NOT_FOUND_EXCEPTION);
        goto finish;
    }

    privKey = PK11_FindKeyByAnyCert(cert, NULL);
    if(privKey == NULL) {
        JSS_throw(env, OBJECT_NOT_FOUND_EXCEPTION);
        goto finish;
    }

    Key = JSS_PK11_wrapPrivKey(env, &privKey);

finish:
    if(privKey != NULL) {
        SECKEY_DestroyPrivateKey(privKey);
    }
    PR_DetachThread();
    return Key;
}


/***********************************************************************
 * Node in linked list of certificates
 */
typedef struct JSScertNode {
    struct JSScertNode *next;
    CERTCertificate *cert;
} JSScertNode;


/***********************************************************************
 *
 * c e r t _ c h a i n _ f r o m _ c e r t
 *
 * Builds a certificate chain from a certificate. Returns a Java array
 * of PK11Certs.
 *
 * INPUTS:
 *      env
 *          The JNI environment. Must not be NULL.
 *      handle
 *          The certificate database in which to search for the certificate
 *          chain.  This should usually be the default cert db. Must not
 *          be NULL.
 *      leaf
 *          A CERTCertificate that is the leaf of the cert chain. Must not
 *          be NULL.
 * RETURNS:
 *      NULL if an exception was thrown, or
 *      A Java array of PK11Cert objects which constitute the chain of
 *      certificates. The chains starts with the one passed in and
 *      continues until either a self-signed root is found or the next
 *      certificate in the chain cannot be found. At least one cert will
 *      be in the chain: the leaf certificate passed in.
 */
static jobjectArray
cert_chain_from_cert(JNIEnv *env, CERTCertDBHandle *handle,
    CERTCertificate *leaf)
{
    CERTCertificate *c;
    int i, len = 0;
    JSScertNode *head=NULL, *tail, *node;
    jobjectArray certArray = NULL;
    jclass certClass;

    PR_ASSERT(env!=NULL && handle!=NULL && leaf!=NULL);

    head = tail = (JSScertNode*) PR_CALLOC( sizeof(JSScertNode) );
    if (head == NULL) goto no_memory;

    /* put primary cert first in the linked list */
    head->cert = c = CERT_DupCertificate(leaf);
    head->next = NULL;
    PR_ASSERT(c != NULL); /* CERT_DupCertificate really can't return NULL */
    len++;

    /*
     * add certs until we come to a self-signed one
     */
    while(SECITEM_CompareItem(&c->derIssuer, &c->derSubject) != SECEqual) {
        c = CERT_FindCertByName(handle, &tail->cert->derIssuer);
        if (c == NULL) break;

        tail->next = (JSScertNode*) PR_CALLOC( sizeof(JSScertNode) );
        tail = tail->next;
        if (tail == NULL) goto no_memory;

        tail->cert = c;
        len++;
    }

    /*
     * Turn the cert chain into a Java array of certificates
     */
    certClass = (*env)->FindClass(env, CERT_CLASS_NAME);
    if(certClass==NULL) {
        ASSERT_OUTOFMEM(env);
        goto finish;
    }
    certArray = (*env)->NewObjectArray(env, len, certClass, (jobject)NULL);
    if(certArray==NULL) {
        ASSERT_OUTOFMEM(env);
        goto finish;
    }
    /* convert linked list to array, freeing the linked list as we go */
    for( i=0; head != NULL; ++i ) {
        jobject certObj;

        node = head;

        PR_ASSERT(i < len);
        PR_ASSERT(node->cert != NULL);

        /* Convert C cert to Java cert */
        certObj = JSS_PK11_wrapCert(env, &node->cert);
        PR_ASSERT( node->cert == NULL );
        if(certObj == NULL) {
            PR_ASSERT( (*env)->ExceptionOccurred(env) != NULL );
            goto finish;
        }

        /* Insert Java cert into array */
        (*env)->SetObjectArrayElement(env, certArray, i, certObj);
        if( (*env)->ExceptionOccurred(env) ) {
            goto finish;
        }

        /* Free this list element */
        head = head->next;
        PR_Free(node);
    }

    goto finish;
no_memory:
    JSS_throw(env, OUT_OF_MEMORY_ERROR);
finish:
    /* Free the linked list of certs if it hasn't been deleted already */
    while(head != NULL) {
        node = head;
        head = head->next;
        if (node->cert != NULL) {
            CERT_DestroyCertificate(node->cert);
        }
        PR_Free(node);
    }

    return certArray;
}

/*****************************************************************
 *
 * CryptoManager. b u i l d C e r t i f i c a t e C h a i n N a t i v e
 *
 * INPUTS:
 *      env
 *          The JNI environment. Must not be NULL.
 *      this
 *          The PK11Finder object. Must not be NULL.
 *      leafCert
 *          A PK11Cert object from which a cert chain will be built.
 *          Must not be NULL.
 * RETURNS:
 *      NULL if an exception occurred, or
 *      An array of PK11Certs, the cert chain, with the leaf at the bottom.
 *      There will always be at least one element in the array (the leaf).
 */
JNIEXPORT jobjectArray JNICALL
Java_org_mozilla_jss_CryptoManager_buildCertificateChainNative
    (JNIEnv *env, jobject this, jobject leafCert)
{
    PRThread *pThread;
    CERTCertificate *leaf;
    jobjectArray chainArray=NULL;
    CERTCertDBHandle *certdb;

    pThread = PR_AttachThread(PR_SYSTEM_THREAD, 0, NULL);
    PR_ASSERT(pThread != NULL);

    PR_ASSERT(env!=NULL && this!=NULL && leafCert!=NULL);

    if( JSS_PK11_getCertPtr(env, leafCert, &leaf) != PR_SUCCESS) {
        JSS_throwMsg(env, CERTIFICATE_EXCEPTION,
            "Could not extract pointer from PK11Cert");
        goto finish;
    }
    PR_ASSERT(leaf!=NULL);

    certdb = CERT_GetDefaultCertDB();
    if(certdb == NULL) {
        PR_ASSERT(PR_FALSE);
        JSS_throwMsg(env, TOKEN_EXCEPTION,
            "No default certificate database has been registered");
        goto finish;
    }

    /* Get the cert chain */
    chainArray = cert_chain_from_cert(env, certdb, leaf);
    if(chainArray == NULL) {
        PR_ASSERT( (*env)->ExceptionOccurred(env) != NULL);
        goto finish;
    }

finish:
    
    PR_DetachThread();
    return chainArray;
}

/***********************************************************************
 * DERCertCollection
 */
typedef struct {
    SECItem *derCerts;
    int numCerts;
} DERCertCollection;

/***********************************************************************
 * c o l l e c t _ c e r t s
 *
 * Copies certs into a new array.
 *
 * 'arg' is a pointer to a DERCertCollection structure, which will be filled in.
 * 'certs' is an array of pointers to SECItems.
 */
static SECStatus
collect_der_certs(void *arg, SECItem **certs, int numcerts)
{
    int itemsCopied=0;
    SECItem *certCopies; /* array of SECItem */
    SECStatus rv;

    PR_ASSERT(arg!=NULL);

    certCopies = PR_MALLOC( sizeof(SECItem) * numcerts);
    ((DERCertCollection*)arg)->derCerts = certCopies;
    ((DERCertCollection*)arg)->numCerts = numcerts;
    if(certCopies == NULL) {
        return SECFailure;
    }
    for(itemsCopied=0; itemsCopied < numcerts; itemsCopied++) {
        rv=SECITEM_CopyItem(NULL, &certCopies[itemsCopied], certs[itemsCopied]);
        if( rv == SECFailure ) {
            goto loser;
        }
    }
    PR_ASSERT(itemsCopied == numcerts);

    return SECSuccess;

loser:
    for(; itemsCopied >= 0; itemsCopied--) {
        SECITEM_FreeItem( &certCopies[itemsCopied], PR_FALSE /*freeit*/);
    }
    PR_Free( certCopies );
    ((DERCertCollection*)arg)->derCerts = NULL;
    ((DERCertCollection*)arg)->numCerts = 0;
    return SECFailure;
}

static SECStatus
ImportCAChain(SECItem *certs, int numcerts, SECCertUsage certUsage)
{
    SECStatus rv;
    SECItem *derCert;
    SECItem certKey;
    PRArenaPool *arena;
    CERTCertificate *cert = NULL;
    CERTCertificate *newcert = NULL;
    CERTCertDBHandle *handle;
    CERTCertTrust trust;
    PRBool isca;
    char *nickname;
    unsigned int certtype;
    
    handle = CERT_GetDefaultCertDB();
    
    arena = NULL;

    arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if ( ! arena ) {
        goto loser;
    }

    while (numcerts--) {
        derCert = certs;
        certs++;
        
        /* get the key (issuer+cn) from the cert */
        rv = CERT_KeyFromDERCert(arena, derCert, &certKey);
        if ( rv != SECSuccess ) {
            goto loser;
        }

        /* same cert already exists in the database, don't need to do
         * anything more with it
         */
        cert = CERT_FindCertByKey(handle, &certKey);
        if ( cert ) {
            CERT_DestroyCertificate(cert);
            cert = NULL;
            continue;
        }

        /* decode my certificate */
        newcert = CERT_DecodeDERCertificate(derCert, PR_FALSE, NULL);
        if ( newcert == NULL ) {
            goto loser;
        }

        /* does it have the CA extension */
        
        /*
         * Make sure that if this is an intermediate CA in the chain that
         * it was given permission by its signer to be a CA.
         */
        isca = CERT_IsCACert(newcert, &certtype);

        if ( isca ) {

            /* SSL ca's must have the ssl bit set */
            if ( ( certUsage == certUsageSSLCA ) &&
                ( ( certtype & NS_CERT_TYPE_SSL_CA ) != NS_CERT_TYPE_SSL_CA ) ){
                goto endloop;
            }

            /* it passed all of the tests, so lets add it to the database */
            /* mark it as a CA */
            PORT_Memset((void *)&trust, 0, sizeof(trust));
            switch ( certUsage ) {
            case certUsageSSLCA:
                trust.sslFlags = CERTDB_VALID_CA;
                break;
            case certUsageUserCertImport:
                if ( ( certtype & NS_CERT_TYPE_SSL_CA ) == NS_CERT_TYPE_SSL_CA){
                    trust.sslFlags = CERTDB_VALID_CA;
                }
                if ( ( certtype & NS_CERT_TYPE_EMAIL_CA ) ==
                    NS_CERT_TYPE_EMAIL_CA ) {
                    trust.emailFlags = CERTDB_VALID_CA;
                }
                if ( ( certtype & NS_CERT_TYPE_OBJECT_SIGNING_CA ) ==
                    NS_CERT_TYPE_OBJECT_SIGNING_CA ) {
                    trust.objectSigningFlags = CERTDB_VALID_CA;
                }
                break;
            default:
                PORT_Assert(0);
                break;
            }
        } else {
            trust.sslFlags = CERTDB_VALID_CA;
            trust.emailFlags =  CERTDB_VALID_CA;
            trust.objectSigningFlags = CERTDB_VALID_CA;
        }
        
        cert = CERT_NewTempCertificate(handle, derCert, NULL, PR_FALSE,
            PR_TRUE);
        if ( cert == NULL ) {
            goto loser;
        }
        
        /* get a default nickname for it */
        nickname = CERT_MakeCANickname(cert);

        rv = CERT_AddTempCertToPerm(cert, nickname, &trust);
        /* free the nickname */
        if ( nickname ) {
            PORT_Free(nickname);
        }

        CERT_DestroyCertificate(cert);
        cert = NULL;
        
        if ( rv != SECSuccess ) {
            goto loser;
        }

endloop:
        if ( newcert ) {
            CERT_DestroyCertificate(newcert);
            newcert = NULL;
        }
        
    }

    rv = SECSuccess;
    goto done;
loser:
    rv = SECFailure;
done:
    
    if ( newcert ) {
        CERT_DestroyCertificate(newcert);
        newcert = NULL;
    }
    
    if ( cert ) {
        CERT_DestroyCertificate(cert);
        cert = NULL;
    }
    
    if ( arena ) {
        PORT_FreeArena(arena, PR_FALSE);
    }

    return(rv);
}


/***********************************************************************
 * CryptoManager.importCertToPerm
 *  - add the certificate to the permanent database 
 *
 * throws TOKEN_EXCEPTION
 */
JNIEXPORT jobject JNICALL
Java_org_mozilla_jss_CryptoManager_importCertToPermNative
    (JNIEnv *env, jobject this, jobject cert, jstring nickString)
{
    SECStatus rv;
    CERTCertificate *newCert;
    CERTCertTrust    trustflags;
    jobject          result=NULL;
    char *nickname=NULL;

    /* first, get the NSS cert pointer from the 'cert' object */

    if ( JSS_PK11_getCertPtr(env, cert, &newCert) != PR_SUCCESS) {
        PR_ASSERT( (*env)->ExceptionOccurred(env) != NULL);
        goto finish;
    }
    PR_ASSERT(newCert != NULL);

    if (nickString != NULL) {
        nickname = (char*) (*env)->GetStringUTFChars(env, nickString, NULL);
    }

    trustflags.sslFlags = 0;
    trustflags.emailFlags = 0;
    trustflags.objectSigningFlags = 0;

    /* Then, add to permanent database */

    rv = CERT_AddTempCertToPerm(newCert, nickname,
               &trustflags);

    if (rv == SECSuccess) {
        /* build return object */
        result = JSS_PK11_wrapCert(env, &newCert);
    }
    else {
        /*  CERT_AddTempCertToPerm does not properly set NSPR
            Error value, so no detail can be retrieved
        */
        JSS_throwMsg(env, TOKEN_EXCEPTION,
                    "Unable to insert certificate into permanent database");


    }
    if (nickname != NULL) {
        (*env)->ReleaseStringUTFChars(env, nickString, nickname);
    }

finish:
    return result;

}


/**
 * Returns 
 *   -1 if operation error.
 *    0 if no leaf found. 
 *    1 if leaf is found
 */
static int find_leaf_cert(
  CERTCertDBHandle *certdb,
  SECItem *derCerts, 
  int numCerts, 
  int *linked,
  int cur_link,
  int *leaf_link
) 
{
      CERTCertificate *curCert = NULL;
      CERTCertificate *theCert = NULL;
      int i;
      int status = 0;

      theCert= CERT_NewTempCertificate(certdb, &derCerts[cur_link], 
                 NULL, PR_FALSE /* isperm */, PR_TRUE /*copyDER*/);
      if (theCert == NULL) {
        status = -1;
        goto finish;
      }
      for (i=0; i<numCerts; i++) {
        if (linked[i] == 1) {
          /* help speeding up the searching */
          continue;
        }
        curCert = CERT_NewTempCertificate(certdb, &derCerts[i], NULL,
              PR_FALSE /* isperm */, PR_TRUE /*copyDER*/);
        if(curCert == NULL) {
          status = -1;
          goto finish;
        }
        if (SECITEM_CompareItem(&theCert->derSubject, 
                                &curCert->derIssuer) == SECEqual) {
          linked[i] = 1;
          *leaf_link = i;
          status = 1; /* got it */
          goto finish;
        } 
        CERT_DeleteTempCertificate(curCert);
        curCert = NULL;
      } /* for */

finish:
      if (theCert != NULL) {
        CERT_DeleteTempCertificate(theCert);
      }
      if (curCert != NULL) {
        CERT_DeleteTempCertificate(curCert);
      }
      return status;
}

/**
 * This function handles unordered certificate chain also.
 * Return:
 *   1 on success
 *   0 otherwise
 */
static int find_leaf_cert_in_chain(
  CERTCertDBHandle *certdb,
  SECItem *derCerts, 
  int numCerts, 
  SECItem *theDerCert
)
{
  int status = 1;
  int found;
  int i;
  int cur_link, leaf_link;
  int *linked = NULL;

  linked = PR_Malloc( sizeof(int) * numCerts );

  /* initialize the bitmap */
  for (i = 0; i < numCerts; i++) {
    linked[i] = 0;
  }

  /* pick the first cert to start with */
  leaf_link = 0;
  cur_link = leaf_link;
  linked[leaf_link] = 1;

  while (((found = find_leaf_cert(certdb, 
     derCerts, numCerts, linked, cur_link, &leaf_link)) == 1))
  {
    cur_link = leaf_link;   
  }
  if (found == -1) {
    /* the certificate chain is problemtic! */
    status = 0; 
    goto finish;
  }
  
  *theDerCert = derCerts[leaf_link]; 

finish:

  if (linked != NULL) {
    PR_Free(linked);
  }

  return status;
} /* find_leaf_cert_in_chain */

/***********************************************************************
 * CryptoManager.importCertPackage
 */
JNIEXPORT jobject JNICALL
Java_org_mozilla_jss_CryptoManager_importCertPackageNative
    (JNIEnv *env, jobject this, jbyteArray packageArray, jstring nickString,
     jboolean noUser, jboolean leafIsCA)
{
    SECItem *derCerts=NULL;
    int certi= -1;
    SECItem theDerCert;
    int numCerts;
    jbyte *packageBytes=NULL;
    jsize packageLen;
    SECStatus status;
    int i, userCertFound = 0;
    DERCertCollection collection;
    CERTCertificate *leafCert=NULL;
    CERTCertDBHandle *certdb = CERT_GetDefaultCertDB();
    CK_OBJECT_HANDLE keyID;
    PK11SlotInfo *slot=NULL;
    char *nickChars = NULL;
    PRBool certExists = PR_FALSE;
    jobject leafObject=NULL;
    CERTIssuerAndSN *issuerAndSN;
    PLArenaPool *arena=NULL;

    /***************************************************
     * Validate arguments
     ***************************************************/
    PR_ASSERT( env!=NULL && this!=NULL );
    if(packageArray == NULL) {
        PR_ASSERT(PR_FALSE);
        JSS_throwMsg(env, CERTIFICATE_ENCODING_EXCEPTION,
            "Certificate package is NULL");
        goto finish;
    }
    PR_ASSERT(certdb != NULL);

    /***************************************************
     * Convert package from byte array to jbyte*
     ***************************************************/
    packageBytes = (*env)->GetByteArrayElements(env, packageArray, NULL);
    if(packageBytes == NULL) {
        PR_ASSERT( (*env)->ExceptionOccurred(env) );
        goto finish;
    }
    packageLen = (*env)->GetArrayLength(env, packageArray);

    /***************************************************
     * Decode package with HCL function
     ***************************************************/
    status = CERT_DecodeCertPackage((char*) packageBytes,
                                    (int) packageLen,
                                    collect_der_certs,
                                    (void*) &collection);
    if( status != SECSuccess  || collection.numCerts < 1 ) {
        if( (*env)->ExceptionOccurred(env) == NULL) {
            JSS_throwMsg(env, CERTIFICATE_ENCODING_EXCEPTION,
                "Security library failed to decode certificate package");
        }
        goto finish;
    }
    derCerts = collection.derCerts;
    numCerts = collection.numCerts;

    /***************************************************
     * convert nickname to char*
     ***************************************************/
    if(nickString == NULL) {
        nickChars = NULL;
    } else {
        nickChars = (char*) (*env)->GetStringUTFChars(env, nickString, NULL);
    }

    /***************************************************
     * user cert can be anywhere in the cert chain. loop and find it.
     * The point is to find the user cert with keys on the db, then
     * treat the other certs in the chain as CA certs to import.
     * The real order of the cert chain shouldn't matter, and shouldn't
     * be assumed, and the real location of this user cert in the chain,
     * if present, shouldn't be assumed either.
     ***************************************************/
    if (numCerts > 1) {
        for (certi=0; certi<numCerts; certi++) {
            leafCert = CERT_NewTempCertificate(certdb, &derCerts[certi], NULL,
                            PR_FALSE /* isperm */, PR_TRUE /*copyDER*/);
            if(leafCert == NULL) {
                JSS_throwMsg(env, CERTIFICATE_ENCODING_EXCEPTION,
                    "Failed to create new temporary certificate");
                goto finish;
            }

            slot = PK11_KeyForCertExists(leafCert, &keyID, NULL);
            if (slot !=NULL) { /* found the use cert */
                theDerCert = derCerts[certi];
                /* delete it so it wouldn't cause conflict */
                CERT_DeleteTempCertificate(leafCert);
                break; /*certi now indicates the location of our user cert in chain*/
            }

            /* delete it so it wouldn't cause conflict */
            CERT_DeleteTempCertificate(leafCert);

        } /* end for */

        /* (NO_USER_CERT_HANDLING)
         Handles the case when the user certificate is not in
         the certificate chain.
        */
        if ((slot == NULL)) { /* same as "noUser = 1" */
            /* #397713 */
            if (!find_leaf_cert_in_chain(certdb, derCerts,
                    numCerts, &theDerCert))
            {
                JSS_throwMsg(env, CERTIFICATE_ENCODING_EXCEPTION,
                    "Failed to locate leaf certificate in chain");
                goto finish;
            }
        }

    } else {/* numCerts <= 1 */
        theDerCert = derCerts[0];
        certi = 0;
    }

    /***************************************************
     * Check to see if this certificate already exists in the database
     ***************************************************/
    if( SEC_CertDBKeyConflict( &theDerCert, certdb ) ) {
      certExists = PR_TRUE;
    }

    /***************************************************
     * Create a new cert structure for the leaf cert
     ***************************************************/
    leafCert = CERT_NewTempCertificate(certdb, &theDerCert, NULL,
                PR_FALSE /* isperm */, PR_TRUE /*copyDER*/);
    if(leafCert == NULL) {
        JSS_throwMsg(env, CERTIFICATE_ENCODING_EXCEPTION,
            "Failed to create new temporary certificate");
        goto finish;
    }
    arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    issuerAndSN = CERT_GetCertIssuerAndSN(arena, leafCert);
    if(issuerAndSN == NULL) {
        PR_ASSERT(PR_FALSE);
        JSS_throw(env, OUT_OF_MEMORY_ERROR);
        goto finish;
    }

    /***************************************************
     * Is this a user cert?
     ***************************************************/
    if(noUser) {
        slot = NULL;
    } else { 
        slot = PK11_KeyForCertExists(leafCert, &keyID, NULL);
    }
    if( slot == NULL ) {
        if( !noUser && !CERT_IsCACert(leafCert, NULL)) {
            /*****************************************
             * This isn't a CA cert, but it also doesn't have a matching
             * key, so it's supposed to be a user cert but it has failed
             * miserably at its calling.
             *****************************************/
            JSS_throw(env, NO_SUCH_ITEM_ON_TOKEN_EXCEPTION);
            goto finish;
        }
    } else {
        /***************************************************
         * We have a user cert, import it 
         ***************************************************/

        /***************************************************
         * Don't re-import a cert that already exists
         ***************************************************/
        if(certExists) {
            /* leaf cert already exists in database */
            JSS_throw(env, USER_CERT_CONFLICT_EXCEPTION);
            goto finish;
        }

        /***************************************************
         * Check for nickname conflicts
         ***************************************************/
        if( SEC_CertNicknameConflict(nickChars,
                                     &leafCert->derSubject,
                                     certdb))
        {
            JSS_throw(env, NICKNAME_CONFLICT_EXCEPTION);
            goto finish;
        }

        /***************************************************
         * Import this certificate onto its token.
         ***************************************************/
        PK11_FreeSlot(slot);
        slot = PK11_ImportCertForKey(leafCert, nickChars, NULL);
        if( slot == NULL ) {
            /* We already checked for this, shouldn't fail here */
            if(PR_GetError() == SEC_ERROR_ADDING_CERT) {
                PR_ASSERT(PR_FALSE);
                JSS_throw(env, NO_SUCH_ITEM_ON_TOKEN_EXCEPTION);
            } else {
                JSS_throw(env, TOKEN_EXCEPTION);
            }
            goto finish;
        }

        if( ! leafIsCA ) {
            ++userCertFound;
        }
    }

    /***************************************************
     * Destroy the leaf cert before calling ImportCAChain.
     * If a cert is already present in the temp database, ImportCAChain
     * will skip it.  So we want to take the leaf cert of the database
     * in case it is a CA cert that needs to be properly imported
     * by ImportCAChain.
     ***************************************************/
    if(leafCert != NULL) {
        CERT_DestroyCertificate(leafCert);
        leafCert = NULL;
    }

    /***************************************************
     * Now add the rest of the certs (which should all be CAs)
     ***************************************************/
    if( numCerts-userCertFound>= 1 ) {

      if (certi == 0) {
        status = ImportCAChain(derCerts+userCertFound,
                                    numCerts-userCertFound,
                                    certUsageUserCertImport);
        if(status != SECSuccess) {
            JSS_trace(env, JSS_TRACE_ERROR,
                "CERT_ImportCAChain returned an error in "
                "CryptoManager.importCertPackage.");
            JSS_throwMsg(env, CERTIFICATE_ENCODING_EXCEPTION,
                "CERT_ImportCAChain returned an error");
            goto finish;
        }
      } else if (certi == numCerts) {
        status = ImportCAChain(derCerts,
                                    numCerts-userCertFound,
                                    certUsageUserCertImport);
        if(status != SECSuccess) {
            JSS_trace(env, JSS_TRACE_ERROR,
                "CERT_ImportCAChain returned an error in "
                "CryptoManager.importCertPackage.");
            JSS_throwMsg(env, CERTIFICATE_ENCODING_EXCEPTION,
                "CERT_ImportCAChain returned an error");
            goto finish;
        }
      } else {
        status = ImportCAChain(derCerts,
                   certi,
                   certUsageUserCertImport);
        if(status != SECSuccess) {
            JSS_trace(env, JSS_TRACE_ERROR,
                "CERT_ImportCAChain returned an error in "
                "CryptoManager.importCertPackage.");
            JSS_throwMsg(env, CERTIFICATE_ENCODING_EXCEPTION,
                "CERT_ImportCAChain returned an error");
            goto finish;
        }

        status = ImportCAChain(derCerts+certi+1,
                   numCerts-certi-1,
                   certUsageUserCertImport);
        if(status != SECSuccess) {
            JSS_trace(env, JSS_TRACE_ERROR,
                "CERT_ImportCAChain returned an error in "
                "CryptoManager.importCertPackage.");
            JSS_throwMsg(env, CERTIFICATE_ENCODING_EXCEPTION,
                "CERT_ImportCAChain returned an error");
            goto finish;
        }

      }

    }

    /***************************************************
     * Now lookup the leaf cert and make it into a Java object.
     * Apparently, the PK11 lookup checks external tokens first,
     * while the CERT lookup checks the database first.  If the leaf is
     * a CA cert, we want to return the copy in the internal database
     * rather than the copy on the token, so we use the CERT call.  We
     * use the PK11 call for user certs that aren't expected to be CAs
     * by the caller.
     ***************************************************/
    if(slot && !leafIsCA) {
        PK11_FreeSlot(slot);
        leafCert = PK11_FindCertByIssuerAndSN(&slot, issuerAndSN, NULL);
    } else {
        leafCert = CERT_FindCertByIssuerAndSN(certdb, issuerAndSN);
    }
    PR_ASSERT( leafCert != NULL );
    leafObject = JSS_PK11_wrapCert(env, &leafCert);

finish:
    if(slot!=NULL) {
        PK11_FreeSlot(slot);
    }
    if(derCerts != NULL) {
        for(i=0; i < numCerts; i++) {
            SECITEM_FreeItem(&derCerts[i], PR_FALSE /*freeit*/);
        }
        PR_Free(derCerts);
    }
    if(packageBytes != NULL) {
        (*env)->ReleaseByteArrayElements(env, packageArray, packageBytes,
                                            JNI_ABORT); /* don't copy back */
    }
    if(leafCert != NULL) {
        CERT_DestroyCertificate(leafCert);
    }
    if(arena != NULL) {
        PORT_FreeArena(arena, PR_FALSE);
    }

    return leafObject;
}

/**********************************************************************
 * PKCS #7 Encoding data structures
 */
typedef struct BufferNodeStr {
    char *data;
    unsigned long len;
    struct BufferNodeStr *next;
} BufferNode;

typedef struct {
    BufferNode *head;
    BufferNode *tail;
    unsigned long totalLen;
} EncoderCallbackInfo;

/**********************************************************************
 * c r e a t e E n c o d e r C a l l b a c k I n f o
 *
 * Constructor for EncoderCallbackInfo structure.
 * Returns NULL if it runs out of memory, otherwise a new EncoderCallbackInfo.
 */
static EncoderCallbackInfo*
createEncoderCallbackInfo()
{
    EncoderCallbackInfo *info;

    info = PR_Malloc( sizeof(EncoderCallbackInfo) );
    if( info == NULL ) {
        return NULL;
    }
    info->head = info->tail = NULL;
    info->totalLen = 0;

    return info;
}

/***********************************************************************
 * d e s t r o y E n c o d e r C a l l b a c k I n f o
 *
 * Destructor for EncoderCallbackInfo structure.
 */
static void
destroyEncoderCallbackInfo(EncoderCallbackInfo *info)
{
    BufferNode *node;

    PR_ASSERT(info != NULL);

    while(info->head != NULL) {
        node = info->head;
        info->head = info->head->next;

        if(node->data) {
            PR_Free(node->data);
        }
        PR_Free(node);
    }
    PR_Free(info);
}

/***********************************************************************
 * e n c o d e r O u t p u t C a l l b a c k
 *
 * Called by the PKCS #7 encoder whenever output is available.
 * Appends the output to a linked list.
 */
static void
encoderOutputCallback( void *arg, const char *buf, unsigned long len)
{
    BufferNode *node;
    EncoderCallbackInfo *info;

    /***************************************************
     * validate arguments 
     ***************************************************/
    PR_ASSERT(arg!=NULL);
    info = (EncoderCallbackInfo*) arg;
    if(len == 0) {
        return;
    }
    PR_ASSERT(buf != NULL);

    /***************************************************
     * Create a new node to store this information
     ***************************************************/
    node = PR_NEW( BufferNode );  
    if( node == NULL ) {
        PR_ASSERT(PR_FALSE);
        goto finish;
    }
    node->len = len;
    node->data = PR_Malloc( len );
    if( node->data == NULL ) {
        PR_ASSERT(PR_FALSE);
        goto finish;
    }
    memcpy( node->data, buf, len );
    node->next = NULL;

    /***************************************************
     * Stick the new node onto the end of the list
     ***************************************************/
    if( info->head == NULL ) {
        PR_ASSERT(info->tail == NULL);

        info->head = info->tail = node;
    } else {
        PR_ASSERT(info->tail != NULL);
        info->tail->next = node;
        info->tail = node;
    }
    node = NULL;

    info->totalLen += len;

finish:
    if(node != NULL) {
        if( node->data != NULL) {
            PR_Free(node->data);
        }
        PR_Free(node);
    }
    return;
}

/***********************************************************************
 * CryptoManager.exportCertsToPKCS7
 */
JNIEXPORT jbyteArray JNICALL
Java_org_mozilla_jss_CryptoManager_exportCertsToPKCS7
    (JNIEnv *env, jobject this, jobjectArray certArray)
{
    int i, certcount;
    SEC_PKCS7ContentInfo *cinfo=NULL;
    CERTCertificate *cert;
    jclass certClass;
    jbyteArray pkcs7ByteArray=NULL;
    jbyte *pkcs7Bytes=NULL;
    EncoderCallbackInfo *info=NULL;
    SECStatus status;

    /**************************************************
     * Validate arguments
     **************************************************/
    PR_ASSERT(env!=NULL && this!=NULL);
    if(certArray == NULL) {
        JSS_throw(env, NULL_POINTER_EXCEPTION);
        goto finish;
    }

    certcount = (*env)->GetArrayLength(env, certArray);
    if(certcount < 1) {
        JSS_throwMsg(env, CERTIFICATE_ENCODING_EXCEPTION,
            "At least one certificate must be passed to exportCertsToPKCS7");
        goto finish;
    }

    /*
     * JNI ID lookup
     */
    certClass = (*env)->FindClass(env, CERT_CLASS_NAME);
    if(certClass == NULL) {
        ASSERT_OUTOFMEM(env);
        goto finish;
    }

    /***************************************************
     * Add each cert to the PKCS #7 context.  Create the context
     * for the first cert.
     ***************************************************/
    for(i=0; i < certcount; i++) {
        jobject certObject;

        certObject = (*env)->GetObjectArrayElement(env, certArray, i);
        if( (*env)->ExceptionOccurred(env) != NULL) {
            goto finish;
        }
        PR_ASSERT( certObject != NULL );

        /*
         * Make sure this is a PK11Cert
         */
        if( ! (*env)->IsInstanceOf(env, certObject, certClass) ) {
            JSS_throwMsg(env, CERTIFICATE_ENCODING_EXCEPTION,
                "Certificate was not a PK11 Certificate");
            goto finish;
        }

        /*
         * Convert it to a CERTCertificate
         */
        if( JSS_PK11_getCertPtr(env, certObject, &cert) != PR_SUCCESS) {
            JSS_trace(env, JSS_TRACE_ERROR,
                "Unable to convert Java certificate to CERTCertificate");
            goto finish;
        }
        PR_ASSERT(cert != NULL);

        if( i == 0 ) {
            /*
             * First certificate: create a new PKCS #7 cert-only context
             */
            PR_ASSERT(cinfo == NULL);
            cinfo = SEC_PKCS7CreateCertsOnly(cert,
                                         PR_FALSE, /* don't include chain */
                                         NULL /* cert db */ );
            if(cinfo == NULL) {
                JSS_throwMsg(env, CERTIFICATE_ENCODING_EXCEPTION,
                    "Failed to create PKCS #7 encoding context");
                goto finish;
            }
        } else {
            /*
             * All remaining certificates: add cert to context
             */
            PR_ASSERT(cinfo != NULL);

            if( SEC_PKCS7AddCertificate(cinfo, cert) != SECSuccess ) {
                JSS_throwMsg(env, CERTIFICATE_ENCODING_EXCEPTION,
                    "Failed to add certificate to PKCS #7 encoding context");
                goto finish;
            }
        }
    }
    PR_ASSERT( i == certcount );
    PR_ASSERT( cinfo != NULL );

    /**************************************************
     * Encode the PKCS #7 context into its DER encoding
     **************************************************/
    info = createEncoderCallbackInfo();
    if(info == NULL) {
        JSS_throw(env, OUT_OF_MEMORY_ERROR);
        goto finish;
    }

    status = SEC_PKCS7Encode(cinfo,
                             encoderOutputCallback,
                             (void*)info,
                             NULL /* bulk key */,
                             NULL /* password function */,
                             NULL /* password function arg */ );
    if( status != SECSuccess ) {
        JSS_throwMsg(env, CERTIFICATE_ENCODING_EXCEPTION,
            "Failed to encode PKCS #7 context");
    }
    /* Make sure we got at least some data from the encoder */
    PR_ASSERT(info->totalLen > 0);
    PR_ASSERT(info->head != NULL);

    /**************************************************
     * Create a new byte array to hold the encoded PKCS #7
     **************************************************/
    pkcs7ByteArray = (*env)->NewByteArray(env, info->totalLen);
    if(pkcs7ByteArray == NULL) {
        ASSERT_OUTOFMEM(env);
        goto finish;
    }
    pkcs7Bytes = (*env)->GetByteArrayElements(env, pkcs7ByteArray, NULL);
    if(pkcs7Bytes == NULL) {
        ASSERT_OUTOFMEM(env);
        goto finish;
    }

    /**************************************************
     * Copy the PKCS #7 encoding into the byte array
     **************************************************/
    {
        BufferNode *node;
        unsigned long processed=0;

        for(node=info->head; node!=NULL; node = node->next) {
            PR_ASSERT(processed < info->totalLen);
            PR_ASSERT(node->data != NULL);
            PR_ASSERT(node->len > 0);
            memcpy(pkcs7Bytes+processed, node->data, node->len);
            processed += node->len;
        }
        PR_ASSERT( processed == info->totalLen );
    }

finish:
    /**************************************************
     * Free allocated resources
     **************************************************/
    if( cinfo != NULL) {
        SEC_PKCS7DestroyContentInfo(cinfo);
    }
    if(pkcs7Bytes != NULL) {
        PR_ASSERT(pkcs7ByteArray != NULL);
        (*env)->ReleaseByteArrayElements(env, pkcs7ByteArray, pkcs7Bytes, 0);
    }
    if( info != NULL ) {
        destroyEncoderCallbackInfo(info);
    }

    /**************************************************
     * Return the PKCS #7 information in a byte array, or NULL if an
     * exception occurred
     **************************************************/
    PR_ASSERT( (*env)->ExceptionOccurred(env)!=NULL || pkcs7ByteArray!=NULL );
    return pkcs7ByteArray;
}

/***********************************************************************
 *
 * Data structures for traversing certificates in the permanent
 * database.
 */
typedef struct CertNode {
    CERTCertificate *cert;
    struct CertNode *next;
} CertNode;

typedef struct {
    CertNode *head;
    CertNode *tail;
    int numCACerts;
    int numCerts;
} CertCollection;

/***********************************************************************
 * c o l l e c t _ c e r t s
 */
static SECStatus
collect_certs(CERTCertificate *cert, SECItem *key, void *arg)
{
    CertCollection *collection = (CertCollection*)arg;
    CertNode *node;
    
    PR_ASSERT( collection != NULL);

    node = PR_MALLOC( sizeof(CertNode) );
    if( node == NULL ) {
        return SECFailure;
    }

    /***************************************************
     * Add the cert to the linked list
     ***************************************************/
    /*node->cert = CERT_DupCertificate(cert);*/
    /*
     * We need to do it this way because the certs that come into this
     * function aren't initialized properly. Many of their fields
     * point to data on the stack, and isperm==0.
     */
    node->cert = CERT_FindCertByKeyNoLocking(CERT_GetDefaultCertDB(),
                                             &cert->certKey);

    node->next = NULL;
    if( collection->head == NULL ) {
        PR_ASSERT( collection->tail == NULL );
        collection->head = collection->tail = node;
    } else {
        PR_ASSERT( collection->tail != NULL );
        collection->tail->next = node;
        collection->tail = node;
    }

    /***************************************************
     * Count the number of CA certs.
     ***************************************************/
    if( CERT_IsCACert(node->cert, NULL) ) {
        collection->numCACerts++;
    }
    collection->numCerts++;

    return SECSuccess;
}

/***********************************************************************
 * CryptoManager.getCACerts
 */
JNIEXPORT jobjectArray JNICALL
Java_org_mozilla_jss_CryptoManager_getCACerts
    (JNIEnv *env, jobject this)
{
    CERTCertDBHandle *certdb = CERT_GetDefaultCertDB();
    SECStatus rv;
    CertCollection collection = { NULL, NULL, 0};
    jobjectArray certArray = NULL;
    CertNode *node;
    jclass certClass;
    int i;
    jobject certObject;

    PR_ASSERT(certdb != NULL);

    /***************************************************
     * Traverse all permanent certificates, building a linked
     * list and counting the number of CA certs.
     ***************************************************/
    rv = SEC_TraversePermCerts(certdb, collect_certs, (void*) &collection);
    if(rv == SECFailure) {
        JSS_trace(env, JSS_TRACE_ERROR,
            "Traversing permanent certificates failed");
        goto finish;
    }

    /**************************************************
     * Create array of Java certificates
     **************************************************/
    certClass = (*env)->FindClass(env, INTERNAL_CERT_CLASS_NAME);
    if(certClass == NULL) {
        ASSERT_OUTOFMEM(env);
        goto finish;
    }

    certArray = (*env)->NewObjectArray( env,
                                        collection.numCACerts,
                                        certClass,
                                        NULL );
    if( certArray == NULL ) {
        ASSERT_OUTOFMEM(env);
        goto finish;
    }
    PR_ASSERT( (*env)->ExceptionOccurred(env) == NULL );


    /**************************************************
     * Put all the CA certs in the array
     **************************************************/
    i = 0;
    while( collection.head != NULL) {
        node = collection.head;

        PR_ASSERT(node->cert != NULL);

        /*
         * Only add CA certs
         */
        if( CERT_IsCACert(node->cert, NULL) ) {
            PR_ASSERT( i < collection.numCACerts );

            certObject = JSS_PK11_wrapCert(env, &(node->cert));
            if( certObject == NULL ) {
                goto finish;
            }
            (*env)->SetObjectArrayElement(env, certArray, i, certObject);
            if( (*env)->ExceptionOccurred(env) ) {
                goto finish;
            }
            ++i;
        }

        /*
         * Delete each node as we traverse it.
         */
        collection.head = collection.head->next;
        if( node->cert != NULL ) {
            CERT_DestroyCertificate(node->cert);
        }
        PR_Free(node);
    }
    PR_ASSERT( i == collection.numCACerts );

finish:
    /* Free any nodes that didn't already get deleted. */
    while( collection.head != NULL ) {
        node = collection.head;
        collection.head = collection.head->next;
        if( node->cert != NULL) {
            CERT_DestroyCertificate(node->cert);
        }
        PR_Free(node);
    }

    return certArray;
}

/***********************************************************************
 * CryptoManager.getPermCerts
 */
JNIEXPORT jobjectArray JNICALL
Java_org_mozilla_jss_CryptoManager_getPermCerts
    (JNIEnv *env, jobject this)
{
    CERTCertDBHandle *certdb = CERT_GetDefaultCertDB();
    SECStatus rv;
    CertCollection collection = { NULL, NULL, 0};
    jobjectArray certArray = NULL;
    CertNode *node;
    jclass certClass;
    int i;
    jobject certObject;

    PR_ASSERT(certdb != NULL);

    /***************************************************
     * Traverse all permanent certificates, building a linked
     * list and counting the number of CA certs.
     ***************************************************/
    rv = SEC_TraversePermCerts(certdb, collect_certs, (void*) &collection);
    if(rv == SECFailure) {
        JSS_trace(env, JSS_TRACE_ERROR,
            "Traversing permanent certificates failed");
        goto finish;
    }

    /**************************************************
     * Create array of Java certificates
     **************************************************/
    certClass = (*env)->FindClass(env, INTERNAL_CERT_CLASS_NAME);
    if(certClass == NULL) {
        ASSERT_OUTOFMEM(env);
        goto finish;
    }

    certArray = (*env)->NewObjectArray( env,
                                        collection.numCerts,
                                        certClass,
                                        NULL );
    if( certArray == NULL ) {
        ASSERT_OUTOFMEM(env);
        goto finish;
    }
    PR_ASSERT( (*env)->ExceptionOccurred(env) == NULL );


    /**************************************************
     * Put all the certs in the array
     **************************************************/
    i = 0;
    while( collection.head != NULL) {
        node = collection.head;

        PR_ASSERT(node->cert != NULL);

        PR_ASSERT( i < collection.numCerts );

        certObject = JSS_PK11_wrapCert(env, &(node->cert));
        if( certObject == NULL ) {
            goto finish;
        }
        (*env)->SetObjectArrayElement(env, certArray, i, certObject);
        if( (*env)->ExceptionOccurred(env) ) {
            goto finish;
        }
        ++i;

        /*
         * Delete each node as we traverse it.
         */
        collection.head = collection.head->next;
        if( node->cert != NULL ) {
            CERT_DestroyCertificate(node->cert);
        }
        PR_Free(node);
    }
    PR_ASSERT( i == collection.numCerts );

finish:
    /* Free any nodes that didn't already get deleted. */
    while( collection.head != NULL ) {
        node = collection.head;
        collection.head = collection.head->next;
        if( node->cert != NULL) {
            CERT_DestroyCertificate(node->cert);
        }
        PR_Free(node);
    }

    return certArray;
}



 /* Imports a CRL, and stores it into the cert7.db
  *
  * @param the DER-encoded CRL.
  */


JNIEXPORT void JNICALL
Java_org_mozilla_jss_CryptoManager_importCRLNative
    (JNIEnv *env, jobject this,
        jbyteArray der_crl, jstring url_jstr, jint rl_type)

{
    CERTCertDBHandle *certdb = CERT_GetDefaultCertDB();
    CERTSignedCrl *crl = NULL;
    SECItem *packageItem = NULL;
    int status = SECFailure;
    char *url;
    char *errmsg = NULL;

    /***************************************************
     * Validate arguments
     ***************************************************/
    PR_ASSERT( env!=NULL && this!=NULL );
    if(der_crl == NULL) {
        PR_ASSERT(PR_FALSE);
        /* XXX need new exception here */
        JSS_throwMsg(env, CERTIFICATE_ENCODING_EXCEPTION,
            "CRL package is NULL");
        goto finish;
    }
    PR_ASSERT(certdb != NULL);

    /* convert CRL byte[] into secitem */

    packageItem = JSS_ByteArrayToSECItem(env, der_crl);
    if ( packageItem == NULL ) {
        goto finish; 
    }
    /* XXX need to deal with if error */

    if (url_jstr != NULL) {
        url = (char*) (*env)->GetStringUTFChars(env, url_jstr, NULL);
        PR_ASSERT(url!=NULL);
    }
    else {
        url = NULL;
    }

    crl = CERT_ImportCRL( certdb, packageItem, url, rl_type, NULL);

    if( crl == NULL ) {
        status = PR_GetError();
        errmsg = NULL;
        switch (status) {
            case SEC_ERROR_OLD_CRL:
            case SEC_ERROR_OLD_KRL:
                /* not an error - leave as NULL */
                errmsg = NULL;
                goto finish;
            case SEC_ERROR_CRL_EXPIRED:
                errmsg = "CRL Expired";
                break;
            case SEC_ERROR_KRL_EXPIRED:
                errmsg = "KRL Expired";
                break;
            case SEC_ERROR_CRL_NOT_YET_VALID:
                errmsg = "CRL Not yet valid";
                break;
            case SEC_ERROR_KRL_NOT_YET_VALID:
                errmsg = "KRL Not yet valid";
                break;
            case SEC_ERROR_CRL_INVALID: 
                errmsg = "Invalid encoding of CRL";
                break;
            case SEC_ERROR_KRL_INVALID: 
                errmsg = "Invalid encoding of KRL";
                break;
            case SEC_ERROR_BAD_DATABASE: 
                errmsg = "Database error";
                break;
            default:
                /* printf("NSS ERROR = %d\n",status);  */
                errmsg = "Failed to import Revocation List";
            }
        if (errmsg) {
            JSS_throwMsg(env, CRL_IMPORT_EXCEPTION, errmsg);
        }
    }

finish:

    if (packageItem) {
        SECITEM_FreeItem(packageItem, PR_TRUE /*freeit*/);
    }

    if(url != NULL) {
        (*env)->ReleaseStringUTFChars(env, url_jstr, url);
    }

    if (crl) {
        SEC_DestroyCrl(crl);
    }
}

