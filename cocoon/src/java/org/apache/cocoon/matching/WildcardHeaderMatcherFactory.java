/*****************************************************************************
 * Copyright (C) The Apache Software Foundation. All rights reserved.        *
 * ------------------------------------------------------------------------- *
 * This software is published under the terms of the Apache Software License *
 * version 1.1, a copy of which has been included  with this distribution in *
 * the LICENSE file.                                                         *
 *****************************************************************************/
package org.apache.cocoon.matching;

import org.apache.avalon.framework.activity.Initializable;

/**
 * @version CVS $Revision: 1.2 $ $Date: 2002/01/12 01:14:33 $
 * @deprecated replaced by WildcardHeaderMatcher - code factories should no longer be used
 */
public class WildcardHeaderMatcherFactory extends WildcardHeaderMatcher
    implements Initializable
{
    public void initialize() {
        getLogger().warn("WildcardHeaderMatcherFactory is deprecated. Please use WildcardHeaderMatcher");
    }
}
