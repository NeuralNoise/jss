// --- BEGIN COPYRIGHT BLOCK ---
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; version 2 of the License.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along
// with this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
//
// (C) 2012 Red Hat, Inc.
// All rights reserved.
// --- END COPYRIGHT BLOCK ---
package org.mozilla.jss.netscape.security.x509;

import javax.xml.bind.annotation.adapters.XmlAdapter;

import org.apache.commons.lang.StringUtils;

/**
 * The RevocationReasonAdapter class provides custom marshaling for RevocationReason.
 *
 * @author Endi S. Dewata
 */
public class RevocationReasonAdapter extends XmlAdapter<String, RevocationReason> {

    public RevocationReason unmarshal(String value) throws Exception {
        return StringUtils.isEmpty(value) ? null : RevocationReason.valueOf(value);
    }

    public String marshal(RevocationReason value) throws Exception {
        return value == null ? null : value.toString();
    }
}
