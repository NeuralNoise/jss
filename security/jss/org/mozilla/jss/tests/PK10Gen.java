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

package com.netscape.jss;

import com.netscape.jss.crypto.*;
import com.netscape.jss.CryptoManager;
import com.netscape.jss.pkcs11.*;
import com.netscape.jss.pkcs11.PK11Token;
import com.netscape.jss.util.*;

public class PK10Gen {
    public static void main(String args[]) {
		CryptoManager manager;
        Password pass1=null, pass2=null;
		char[] passchar1 = {'f', 'o', 'o', 'b', 'a', 'r'};
		char[] passchar2 = {'n', 'e', 't', 's', 'c', 'a', 'p', 'e'};

        if(args.length != 2) {
            System.err.println("Usage: java com.netscape.jss.PK10Gen <dbdir> [rsa|dsa]");
            return;
        }

		try {
			CryptoManager.initialize(args[0]+"/secmod.db", args[0]+"/key3.db", args[0]+"/cert7.db");
			/*
			CryptoManager.initialize("secmod.db", "key3.db", "cert7.db");
			CryptoManager cm = CryptoManager.getInstance();
			PK11Token token = (PK11Token)cm.getInternalCryptoToken();
			*/
			/*
        CryptoManager.InitializationValues vals = new
            CryptoManager.InitializationValues( args[0]+"/secmodule.db",
                                                args[0]+"/key3.db",
				                                args[0]+"/cert7.db");
        CryptoManager.initialize(vals);
			*/
        try {
            manager = CryptoManager.getInstance();
        } catch( CryptoManager.NotInitializedException e ) {
            System.out.println("CryptoManager not initialized");
            return;
        }

		CryptoToken token = (PK11Token) manager.getInternalKeyStorageToken();
            if(token.isLoggedIn() == false) {
                System.out.println("Good, isLoggedIn correctly says we're"+
                    " not logged in");
            } else {
                System.out.println("ERROR: isLoggedIn incorrectly says we're"+
                    " logged in");
            }

			pass1 = new Password( (char[]) passchar1.clone());
			pass2 = new Password( new char[]{0} );
            token.initPassword(pass2, pass1);
			pass1.clear();
			pass2.clear();
            System.out.println("initialized PIN");
            token.login(pass1);
            System.out.println("logged in");

			String blob = token.generateCertRequest("cn=christina Fu",
												512,
													args[1],
													(byte[]) null,
													(byte[]) null,
													(byte[]) null);
			System.out.println("pkcs#10 blob = \n" + blob);
		} catch(Exception e) {
			System.out.println("exception caught in PK10Gen: " +
							   e.getMessage());
			e.printStackTrace();
		}
	}
}
