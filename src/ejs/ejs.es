
/******************************************************************************/
/* 
 *  This file is an amalgamation of all the individual source code files for
 *  Embedthis Ejscript 1.0.0.
 *
 *  Catenating all the source into a single file makes embedding simpler and
 *  the resulting application faster, as many compilers can do whole file
 *  optimization.
 *
 *  If you want to modify ejs, you can still get the whole source
 *  as individual files if you need.
 */


/************************************************************************/
/*
 *  Start of file "../src/es/core/Array.es"
 */
/************************************************************************/

/**
 *  Array.es - Array class
 *
 *  Copyright (c) All Rights Reserved. See details at the end of the file.
 */

module ejs {

    use default namespace intrinsic

    /**
     *  Arrays provide a growable, integer indexed, in-memory store for objects. An array can be treated as a 
     *  stack (FIFO or LIFO) or a list (ordered). Insertions can be done at the beginning or end of the stack or at an 
     *  indexed location within a list.
     *  @stability evolving
     */
    dynamic native class Array {

        use default namespace public

        /**
         *  Create a new array.
         *  @param values Initialization values. The constructor allows three forms:
         *  <ul>
         *      <li>Array()</li>
         *      <li>Array(size: Number)</li>
         *      <li>Array(elt: Object, ...args)</li>
         *  </ul>
         */
        native function Array(...values)

        /**
         *  Append an item to the array.
         *  @param obj Object to add to the array 
         *  @return The array itself.
         *  @spec ejs
         */
        native function append(obj: Object): Array

        /**
         *  Clear an array. Remove all elements of the array.
         *  @spec ejs
         */
        native function clear() : Void

        /**
         *  Clone the array and all its elements.
         *  @param deep If true, do a deep copy where all object references are also copied, and so on, recursively.
         *  @spec ejs
         */
        override native function clone(deep: Boolean = true) : Array

        /**
         *  Compact an array. Remove all null elements.
         *  @spec ejs
         */
        native function compact() : Array

        /**
         *  Concatenate the supplied elements with the array to create a new array. If any arguments specify an 
         *  array, their elements are concatenated. This is a one level deep copy.
         *  @param args A variable length set of values of any data type.
         *  @return A new array of the combined elements.
         */
        native function concat(...args): Array 

        /**
         *  See if the array contains an element using strict equality "===". This call searches from the start of the 
         *  array for the specified element.  
         *  @param element The object to search for.
         *  @return True if the element is found. Otherwise false.
         *  @spec ejs
         */
        function contains(element: Object): Boolean {
            if (indexOf(element) >= 0) {
                return true
            } else {
                return false
            }
        }

        /**
         *  Determine if all elements match.
         *  Iterate over every element in the array and determine if the matching function is true for all elements. 
         *  This method is lazy and will cease iterating when an unsuccessful match is found. The checker is called 
         *  with the following signature:
         *      function match(element: Object, elementIndex: Number, arr: Array): Boolean
         *  @param match Matching function
         *  @return True if the match function always returns true.
         */
        function every(match: Function): Boolean {
            for (let i: Number in this) {
                if (!match(this[i], i, this)) {
                    return false
                }
            }
            return true
        }

        /**
         *  Find all matching elements.
         *  Iterate over all elements in the object and find all elements for which the matching function is true.
         *  The match is called with the following signature:
         *      function match(element: Object, elementIndex: Number, arr: Array): Boolean
         *  This method is identical to the @findAll method.
         *  @param match Matching function
         *  @return Returns a new array containing all matching elements.
         */
        function filter(match: Function): Array
            findAll(match)

        /**
         *  Find the first matching element.
         *  Iterate over elements in the object and select the first element for which the matching function is true.
         *  The matching function is called with the following signature:
         *      function match(element: Object, elementIndex: Number, arr: Array): Boolean
         *  @param match Matching function
         *  @return True when the match function returns true.
         *  @spec ejs
         */
        function find(match: Function): Object {
            var result: Array = new Array
            for (let i: Number in this) {
                if (match(this[i], i, this)) {
                    return this[i]
                }
            }
            return result
        }

        /**
         *  Find all matching elements.
         *  Iterate over all elements in the object and find all elements for which the matching function is true.
         *  The matching function is called with the following signature:
         *      function match(element: Object, elementIndex: Number, arr: Array): Boolean
         *  @param match Matching function
         *  @return Returns an array containing all matching elements.
         *  @spec ejs
         */
        function findAll(match: Function): Array {
            var result: Array = new Array
            for (let i: Number in this) {
                if (match(this[i], i, this)) {
                    result.append(this[i])
                }
            }
            return result
        }

        /**
         *  Transform all elements.
         *  Iterate over the elements in the array and transform all elements by applying the transform function. 
         *  The matching function is called with the following signature:
         *      function match(element: Object, elementIndex: Number, arr: Array): Boolean
         *  This method is identical to the @transform method.
         *  @param modifier Transforming function
         *  @return Returns a new array of transformed elements.
         */
        function forEach(modifier: Function, thisObj: Object = null): Void {
            transform(modifier)
        }

        /**
         *  Iterator for this array to be used by "for (v in array)"
         */
        override iterator native function get(): Iterator

        /**
         *  Iterator for this array to be used by "for each (v in array)"
         */
        override iterator native function getValues(): Iterator

        /**
         *  Search for an item using strict equality "===". This call searches from the start of the array for 
         *  the specified element.  
         *  @param element The object to search for.
         *  @param startIndex Where in the array to start searching for the object (Defaults to zero). If the index 
         *      is negative, it is taken as an offset from the end of the array. If the calculated index is less than 
         *      zero the entire array is searched. If the index is greater than the length of the array, -1 is returned.
         *  @return The items index into the array if found, otherwise -1.
         */
        native function indexOf(element: Object, startIndex: Number = 0): Number

        /**
         *  Insert elements. Insert elements at the specified position. The insertion occurs before the element at the 
         *      specified position. Negative indicies will measure from the end so that -1 will specify the last element.  
         *      Indicies greater than the array length will append to the end. Indicies before the first position will
         *      insert at the start.
         *  @param pos Where in the array to insert the objects.
         *  @param ...args Arguments are aggregated and passed to the method in an array.
         *  @return The original array.
         *  @spec ejs
         */
        native function insert(pos: Number, ...args): Array

        /**
         *  Convert the array into a string.
         *  Joins the elements in the array into a single string by converting each element to a string and then 
         *  concatenating the strings inserting a separator between each.
         *  @param sep Element separator.
         *  @return A string.
         */
        native function join(sep: String = undefined): String

        /**
         *  Find an item searching from the end of the arry.
         *  Search for an item using strict equality "===". This call searches from the end of the array for the given 
         *  element.
         *  @param element The object to search for.
         *  @param startIndex Where in the array to start searching for the object (Defaults to the array's length).
         *      If the index is negative, it is taken as an offset from the end of the array. If the calculated index 
         *      is less than zero, -1 is returned. If the index is greater or equal to the length of the array, the
         *      whole array will be searched.
         *  @return The items index into the array if found, otherwise -1.
         */
        native function lastIndexOf(element: Object, startIndex: Number = 0): Number

        /**
         *  Length of an array.
         */
        override native function get length(): Number

        /**
         *  Set the length of an array. The array will be truncated if required. If the new length is greater then 
         *  the old length, new elements will be created as required and set to undefined. If the new length is less
         *  than 0 the length is set to zero.
         *  @param value The new length
         */
        native function set length(value: Number): Void

        /**
         *  Call the mapper on each array element in increasing index order and return a new array with the values returned 
         *  from the mapper. The mapper function is called with the following signature:
         *      function mapper(element: Object, elementIndex: Number, arr: Array): Object
         *  @param mapper Transforming function
         */
        function map(mapper: Function): Array {
            var result: Array  = clone()
            result.transform(mapper)
            return result
        }

        /**
         *  Remove and return the last value in the array.
         *  @return The last element in the array. Returns undefined if the array is empty
         */
        native function pop(): Object 

        /**
         *  Append items to the end of the array.
         *  @param items Items to add to the array.
         *  @return The new length of the array.
         */
        native function push(...items): Number 

        /**
         *  Find non-matching elements. Iterate over all elements in the array and select all elements for which 
         *  the filter function returns false. The matching function is called with the following signature:
         *
         *      function match(element: Object, elementIndex: Number, arr: Array): Boolean
         *
         *  @param match Matching function
         *  @return A new array of non-matching elements.
         *  @spec ejs
         */
        function reject(match: Function): Array {
            var result: Array = new Array
            for (let i: Number in this) {
                if (!match(this[i], i, this)) {
                    result.append(this[i])
                }
            }
            return result
        }

        /**
         *  Remove elements. Remove the elements from $start to $end inclusive. The elements are removed and not just set 
         *  to undefined as the delete operator will do. Indicies are renumbered. NOTE: this routine delegates to splice.
         *  @param start Numeric index of the first element to remove. Negative indices measure from the end of the array.
         *  -1 is the last element.
         *  @param end Numeric index of the last element to remove
         *  @spec ejs
         */
        function remove(start: Number, end: Number = -1): Void {
            if (start < 0) {
                start += length
            }
            if (end < 0) {
                end += length
            }
            splice(start, end - start + 1)
        }

        /**
         *  Reverse the order of the objects in the array. The elements are reversed in the original array.
         *  @return A reference to the array.
         */
        native function reverse(): Array 

        /**
         *  Remove and return the first element in the array.
         *  @returns the previous first element in the array.
         */
        native function shift(): Object 

        /**
            Create a new array subset by taking a slice from an array.
            @param start The array index at which to begin. Negative indicies will measure from the end so that -1 will 
                specify the last element. If start is greater than or equal to end, the call returns an empty array.
            @param end The array index at which to end. This is one beyond the index of the last element to extract. If 
                end is negative, it is measured from the end of the array, so use -1 to mean up to but not including the 
                last element of the array.
            @param step Slice every step (nth) element. The step value may be negative.
            @return A new array that is a subset of the original array.
         */
        native function slice(start: Number, end: Number = -1, step: Number = 1): Array 

        /**
         *  Determine if some elements match.
         *  Iterate over all element in the array and determine if the matching function is true for at least one element. 
         *  This method is lazy and will cease iterating when a successful match is found.
         *  The match function is called with the following signature:
         *      function match(element: Object, elementIndex: Number, arr: Array): Boolean
         *  @param match Matching function
         *  @return True if the match function ever is true.
         */
        function some(match: Function): Boolean {
            var result: Array = new Array
            for (let i: Number in this) {
                if (match(this[i], i, this)) {
                    return true
                }
            }
            return false
        }

        /**
         *  Sort the array using the supplied compare function
         *  @param compare Function to use to compare. A null comparator will use a text compare
         *  @param order If order is >= 0, then an ascending order is used. Otherwise descending.
         *  @return the sorted array reference
         *      type Comparator = (function (*,*): AnyNumber | undefined)
         *  @spec ejs Added the order argument.
         */
        native function sort(compare: Function = null, order: Number = 1): Array 

        /**
         *  Insert, remove or replace array elements. Splice modifies an array in place. 
         *  @param start The array index at which the insertion or deleteion will begin. Negative indicies will measure 
         *      from the end so that -1 will specify the last element.  
         *  @param deleteCount Number of elements to delete. If omitted, splice will delete all elements from the 
         *      start argument to the end.  
         *  @param values The array elements to add.
         *  @return Array containing the removed elements.
         */
        native function splice(start: Number, deleteCount: Number, ...values): Array 

        /**
         *  Convert an array to an equivalent JSON encoding.
         *  @return This function returns an array literal string.
         *  @throws TypeError If the array could not be converted to a string.
         *
         *  NOTE: currently using Object.toJSON for this capability
         */ 
        #FUTURE
        override native function toJSON(): String

        /**
         *  Convert the array to a single string each member of the array has toString called on it and the resulting 
         *  strings are concatenated.
         *  @return A string
         */
        override native function toString(locale: String = null): String 

        /**
         *  Transform all elements.
         *  Iterate over all elements in the object and transform the elements by applying the transform function. 
         *  This modifies the object elements in-situ. The transform function is called with the following signature:
         *      function mapper(element: Object, elementIndex: Number, arr: Array): Object
         *  @param mapper Transforming function
         *  @spec ejs
         */
        function transform(mapper: Function): Void {
            for (let i: Number in this) {
                this[i] = mapper(this[i], i, this);
            }
        }

        /**
         *  Remove duplicate elements and make the array unique. Duplicates are detected by using "==" (ie. content 
         *      equality, not strict equality).
         *  @return The original array with unique elements
         *  @spec ejs
         */
        native function unique(): Array

        /**
         *  Insert the items at the front of the array.
         *  @param items to insert
         *  @return Returns the array reference
         */
        function unshift(...items): Object
            insert(0, items)

        /**
         *  Array intersection. Return the elements that appear in both arrays. 
         *  @param arr The array to join.
         *  @return A new array.
         *  @spec ejs
         */
        # DOC_ONLY
        native function & (arr: Array): Array

        /**
         *  Append. Appends elements to an array.
         *  @param elements The array to add append.
         *  @return The modified original array.
         *  @spec ejs
         */
        # DOC_ONLY
        native function << (elements: Array): Array

        /**
         *  Array subtraction. Remove any items that appear in the supplied array.
         *  @param arr The array to remove.
         *  @return A new array.
         *  @spec ejs
         */
        # DOC_ONLY
        native function - (arr: Array): Array

        /**
         *  Array union. Return the union of two arrays. 
         *  @param arr The array to join.
         *  @return A new array
         *  @spec ejs
         */
        # DOC_ONLY
        native function | (arr: Array): Array

        /**
         *  Concatenate two arrays. 
         *  @param arr The array to add.
         *  @return A new array.
         *  @spec ejs
         */
        # DOC_ONLY
        native function + (arr: Array): Array
    }
}


/*
 *  @copy   default
 *  
 *  Copyright (c) Embedthis Software LLC, 2003-2010. All Rights Reserved.
 *  Copyright (c) Michael O'Brien, 1993-2010. All Rights Reserved.
 *  
 *  This software is distributed under commercial and open source licenses.
 *  You may use the GPL open source license described below or you may acquire 
 *  a commercial license from Embedthis Software. You agree to be fully bound 
 *  by the terms of either license. Consult the LICENSE.TXT distributed with 
 *  this software for full details.
 *  
 *  This software is open source; you can redistribute it and/or modify it 
 *  under the terms of the GNU General Public License as published by the 
 *  Free Software Foundation; either version 2 of the License, or (at your 
 *  option) any later version. See the GNU General Public License for more 
 *  details at: http://www.embedthis.com/downloads/gplLicense.html
 *  
 *  This program is distributed WITHOUT ANY WARRANTY; without even the 
 *  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
 *  
 *  This GPL license does NOT permit incorporating this software into 
 *  proprietary programs. If you are unable to comply with the GPL, you must
 *  acquire a commercial license to use this software. Commercial licenses 
 *  for this software and support services are available from Embedthis 
 *  Software at http://www.embedthis.com 
 *  
 *  @end
 */
/************************************************************************/
/*
 *  End of file "../src/es/core/Array.es"
 */
/************************************************************************/



/************************************************************************/
/*
 *  Start of file "../src/es/core/Block.es"
 */
/************************************************************************/

/*
 *  Block.es -- Block scope class used internally by the VM.
 *
 *  Copyright (c) All Rights Reserved. See details at the end of the file.
 */

module ejs {

    use default namespace intrinsic

    /**
     *  The Block type is used to represent program block scope. It is used internally and should not be 
     *  instantiated by user programs.
     *  @spec ejs
     *  @stability stable
     *  @hide
     */
    native final class Block { }
}


/*
 *  @copy   default
 *  
 *  Copyright (c) Embedthis Software LLC, 2003-2010. All Rights Reserved.
 *  Copyright (c) Michael O'Brien, 1993-2010. All Rights Reserved.
 *  
 *  This software is distributed under commercial and open source licenses.
 *  You may use the GPL open source license described below or you may acquire 
 *  a commercial license from Embedthis Software. You agree to be fully bound 
 *  by the terms of either license. Consult the LICENSE.TXT distributed with 
 *  this software for full details.
 *  
 *  This software is open source; you can redistribute it and/or modify it 
 *  under the terms of the GNU General Public License as published by the 
 *  Free Software Foundation; either version 2 of the License, or (at your 
 *  option) any later version. See the GNU General Public License for more 
 *  details at: http://www.embedthis.com/downloads/gplLicense.html
 *  
 *  This program is distributed WITHOUT ANY WARRANTY; without even the 
 *  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
 *  
 *  This GPL license does NOT permit incorporating this software into 
 *  proprietary programs. If you are unable to comply with the GPL, you must
 *  acquire a commercial license to use this software. Commercial licenses 
 *  for this software and support services are available from Embedthis 
 *  Software at http://www.embedthis.com 
 *  
 *  @end
 */
/************************************************************************/
/*
 *  End of file "../src/es/core/Block.es"
 */
/************************************************************************/



/************************************************************************/
/*
 *  Start of file "../src/es/core/Boolean.es"
 */
/************************************************************************/

/*
 *  Boolean.es -- Boolean class
 *
 *  Copyright (c) All Rights Reserved. See details at the end of the file.
 */

module ejs {

    use default namespace intrinsic

    /**
     *  Boolean class. The Boolean class is used to create two immutable boolean values: "true" and "false".
     *  @stability stable
     */
    native final class Boolean {

        /**
            Boolean constructor. Construct a Boolean object and initialize its value. Since Boolean values are 
            immutable, this constructor will return a reference to either the "true" or "false" values.
            @param value. Optional value to use in creating the Boolean object. If the value is omitted or 0, -1, NaN,
                false, null, undefined or the empty string, then the object will be created and set to false.
         */
        native function Boolean(value: Boolean = false)
    }
}


/*
 *  @copy   default
 *  
 *  Copyright (c) Embedthis Software LLC, 2003-2010. All Rights Reserved.
 *  Copyright (c) Michael O'Brien, 1993-2010. All Rights Reserved.
 *  
 *  This software is distributed under commercial and open source licenses.
 *  You may use the GPL open source license described below or you may acquire 
 *  a commercial license from Embedthis Software. You agree to be fully bound 
 *  by the terms of either license. Consult the LICENSE.TXT distributed with 
 *  this software for full details.
 *  
 *  This software is open source; you can redistribute it and/or modify it 
 *  under the terms of the GNU General Public License as published by the 
 *  Free Software Foundation; either version 2 of the License, or (at your 
 *  option) any later version. See the GNU General Public License for more 
 *  details at: http://www.embedthis.com/downloads/gplLicense.html
 *  
 *  This program is distributed WITHOUT ANY WARRANTY; without even the 
 *  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
 *  
 *  This GPL license does NOT permit incorporating this software into 
 *  proprietary programs. If you are unable to comply with the GPL, you must
 *  acquire a commercial license to use this software. Commercial licenses 
 *  for this software and support services are available from Embedthis 
 *  Software at http://www.embedthis.com 
 *  
 *  @end
 */
/************************************************************************/
/*
 *  End of file "../src/es/core/Boolean.es"
 */
/************************************************************************/



/************************************************************************/
/*
 *  Start of file "../src/es/core/ByteArray.es"
 */
/************************************************************************/

/*
 *  ByteArray.es - ByteArray class
 *
 *  Copyright (c) All Rights Reserved. See details at the end of the file.
 */

module ejs {

    use default namespace intrinsic

    /**
     *  ByteArrays provide a growable, integer indexed, in-memory store for bytes. ByteArrays are a powerful data 
     *  type that can be used as a simple array to store and encode data as bytes or it can be used as a Stream 
     *  implementing the Stream interface.
     *  <br/><br/>
     *  When used as a simple byte array, the ByteArray class offers a low level set of methods to insert and 
     *  extract bytes. The index operator [] can be used to access individual bytes and the copyIn and copyOut methods 
     *  can be used to get and put blocks of data. In this mode, the $readPosition and $writePosition properties are 
     *  ignored. Accesses to the byte array are from index zero up to the size defined by the length property. When 
     *  constructed, the ByteArray can be designated as growable, in which case the initial size will grow as required to 
     *  accomodate data and the length property will be updated accordingly.
     *  <br/><br/>
     *  When used as a Stream, the byte array offers various read and write methods which store data at the location 
     *  specified by the write position property and they read data from the $readPosition. The available method 
     *  indicates how much data is available between the read and write position pointers. The flush method will 
     *  reset the pointers to the start of the array. The length property is unchanged in behavior from when used as 
     *  a simple byte array and it specifies the overall storage capacity of the byte array. As numeric values are 
     *  read or written, they will be encoded according to the value of the endian property which can be set to 
     *  either LittleEndian or BigEndian. When used with for/in, ByteArrays will iterate or enumerate over the 
     *  available data between the read and write pointers.
     *  <br/><br/>
     *  In Stream mode ByteArrays can be configured with input and output callbacks to provide or consume data to other 
     *  streams or components. These callbacks will automatically be invoked as required when the various read/write 
     *  methods are called.
     *  @stability evolving
     *  @spec ejs
     */
    native final class ByteArray implements Stream {

        use default namespace public

        /**
         *  Little endian byte order constants used for the $endian property
         */
        static const LittleEndian: Number   = 0

        /**
         *  Big endian byte order used for the $endian property
         */
        static const BigEndian: Number      = 1

        /**
         *  Create a new array. This will set the default encoding.
         *  @param size The initial size of the byte array. If not supplied a system default buffer size will be used.
         *  @param growable Set to true to automatically grow the array as required to fit written data. If growable 
         *      is false, then some writes may return "short". ie. not be able to accomodate all written data.
         */
        native function ByteArray(size: Number = -1, growable: Boolean = false)

        /**
         *  Number of bytes that are currently available for reading. This consists of the bytes available
         *  from the current $readPosition up to the current write position.
         */
        native function get available(): Number 

        /**
         *  Close the byte array
         *  @param graceful If true, then write all pending data
         */
        function close(graceful: Boolean = false): Void {
            if (graceful) {
                flush(graceful)
            }
        }

        /**
         *  Compact available data down and adjust the read/write positions accordingly. This moves available room to
         *  the end of the byte array.
         */
        native function compact(): Void

        /**
         *  Compress the array contents
         */
        # FUTURE
        native function compress(): Void

        /**
         *  Copy data into the array. Data is written at the $destOffset index. This call does not update the 
         *      read and write positions.
         *  @param destOffset Index in the destination byte array to copy the data to
         *  @param src Source byte array containing the data elements to copy
         *  @param srcOffset Location in the source buffer from which to copy the data. Defaults to the start.
         *  @param count Number of bytes to copy. Set to -1 to read to the end of the src buffer.
         *  @return the number of bytes written into the array
         */
        native function copyIn(destOffset: Number, src: ByteArray, srcOffset: Number = 0, count: Number = -1): Number

        /**
         *  Copy data from the array. Data is copied from the $srcOffset pointer. This call does not update the 
         *      read and write positions.
         *  @param srcOffset Location in the source array from which to copy the data.
         *  @param dest Destination byte array
         *  @param destOffset Location in the destination array to copy the data. Defaults to the start.
         *  @param count Number of bytes to read. Set to -1 to read all available data.
         *  @returns the count of bytes read. Returns 0 on end of file.
         *  @throws IOError if an I/O error occurs.
         */
        native function copyOut(srcOffset: Number, dest: ByteArray, destOffset: Number = 0, count: Number = -1): Number

        /**
            Current byte ordering for storing and retrieving numbers. Set to either LittleEndian or BigEndian
         */
        native function get endian(): Number

        /**
            Current byte ordering for storing and retrieving numbers. Set to either LittleEndian or BigEndian
         *  @param value Set to true for little endian encoding or false for big endian.
         */
        native function set endian(value: Number): Void

        /** 
         *  Flush the the byte array and reset the read and write position pointers. This may invoke the output callback
         *  to send the data if the output callback is defined.
         */
        native function flush(graceful: Boolean = true): Void

        /**
         *  Iterator for this array to be used by "for (v in array)". This will return array indicies for 
         *  read data in the array.
         *  @return An iterator object.
         */
        override iterator native function get(): Iterator

        /**
         *  Iterator for this array to be used by "for each (v in array)". This will return read data in the array.
         */
        override iterator native function getValues(): Iterator

        /**
         *  Input callback function when read data is required. The input callback should write to the supplied buffer.
         *  @param callback Function to call to supply read data. The function is called with the following signature:
         *      function inputCallback(buffer: ByteArray): Void
         */
        native function set input(callback: Function): Void

        /**  
            @hide
         */
        native function get input(): Function

        /**
         *  Length of an array. This is not the amount of read or write data, but is the size of the total 
         *      array storage.
         */
        override native function get length(): Number

        #FUTURE
        native function get MD5(): Number

        /**
         *  Define an output function to process (output) data. The output callback should read from the supplied buffer.
         *  @param callback Function to invoke when the byte array is full or flush() is called.
         *      function outputCallback(buffer: ByteArray): Number
         */
        native function set output(callback: Function): Void

        /** */
        native function get output(): Function

        /**
         *  Read data from the array into another byte array. Data is read from the current read $position pointer toward
         *      the current write position. This byte array's $readPosition is updated. If offset is < 0, then 
         *       data is copied to the destination buffer's write position and the destination buffer's write position 
         *       is also updated. If the offset is >= 0, the read and write positions of the destination buffer are updated.
         *  @param buffer Destination byte array
         *  @param offset Location in the destination buffer to copy the data. If the offset is < 0, then the write 
         *      position is used and will be updated with the number of bytes read from the buffer.
         *  @param count Number of bytes to read. Set to -1 to read all available data that will fit into the 
         *      destination buffer.
         *  @returns the count of bytes read. Returns 0 on end of file.
         *  @throws IOError if an I/O error occurs.
         */
        native function read(buffer: ByteArray, offset: Number = 0, count: Number = -1): Number

        /**
         *  Read a boolean from the array. Data is read from the current read $position pointer.
         *  @returns a boolean
         *  @throws IOError if an I/O error occurs or a premature end of file.
         */
        native function readBoolean(): Boolean

        /**
         *  Read a byte from the array. Data is read from the current read $position pointer.
         *  @returns a byte
         *  @throws IOError if an I/O error occurs or a premature end of file.
         */
        native function readByte(): Number

        /**
         *  Read a date from the array or a premature end of file. Data is read from the current read $position pointer.
         *  @returns a date
         *  @throws IOError if an I/O error occurs.
         */
        native function readDate(): Date

        /**
         *  Read a double from the array. The data will be decoded according to the encoding property.
         *  Data is read from the current read $position pointer.
         *  @returns a double
         *  @throws IOError if an I/O error occurs or a premature end of file.
         */
        native function readDouble(): Date

        /**
         *  Read an 32-bit integer from the array. The data will be decoded according to the encoding property.
         *  Data is read from the current read $position pointer.
         *  @returns an integer
         *  @throws IOError if an I/O error occurs or a premature end of file.
         */
        native function readInteger(): Number

        /**
         *  Read a 64-bit long from the array.The data will be decoded according to the encoding property.
         *  Data is read from the current read $position pointer.
         *  @returns a long
         *  @throws IOError if an I/O error occurs or a premature end of file.
         */
        native function readLong(): Number

        /**
         *  Current read position offset
         */
        native function get readPosition(): Number

        /**
         *  Set the current read position offset
         *  @param position The new read position
         */
        native function set readPosition(position: Number): Void

        /**
         *  Read a 16-bit short integer from the array.The data will be decoded according to the encoding property.
         *  Data is read from the current read $position pointer.
         *  @returns a short int
         *  @throws IOError if an I/O error occurs or a premature end of file.
         */
        native function readShort(): Number

        /**
         *  Read a data from the array as a string. Read data from the $readPosition to a string up to the $writePosition,
         *      but not more than count characters.
         *  @param count of bytes to read. If -1, convert the data up to the $writePosition.
         *  @returns a string
         *  @throws IOError if an I/O error occurs or a premature end of file.
         */
        native function readString(count: Number = -1): String

        /**
         *  Read an XML document from the array. Data is read from the current read $position pointer.
         *  @returns an XML document
         *  @throws IOError if an I/O error occurs or a premature end of file.
         */
        native function readXML(): XML

        /**
         *  Reset the read and $writePosition pointers if there is no available data.
         */
        native function reset(): Void

        /**
         *  Number of data bytes that the array can store from the $writePosition till the end of the array.
         */
        native function get room(): Number 

        /**
         *  Convert the data in the byte array between the $readPosition and $writePosition offsets.
         *  @return A string
         */
        override native function toString(locale: String = null): String 

        /**
         *  Uncompress the array
         */
        # FUTURE
        native function uncompress(): Void

        /**
         *  Write data to the array. Binary data is written in an optimal, platform dependent binary format. If 
         *      cross-platform portability is required, use the BinaryStream to encode the data. Data is written to 
         *      the current $writePosition If the data argument is itself a ByteArray, the available data from the 
         *      byte array will be copied. NOTE: the data byte array will not have its readPosition adjusted.
         *  @param data Data elements to write
         *  @return the number of bytes written into the array
         */
        native function write(...data): Number

        /**
         *  Write a byte to the array. Data is written to the current write $position pointer which is then incremented.
         *  @param data Data to write
         */
        native function writeByte(data: Number): Void

        /**
         *  Write a short to the array. Data is written to the current write $position pointer which is then incremented.
         *  @param data Data to write
         */
        native function writeShort(data: Number): Void

        /**
         *  Write a double to the array. Data is written to the current write $position pointer which is then incremented.
         *  @param data Data to write
         */
        native function writeDouble(data: Number): Void

        /**
         *  Write a 32-bit integer to the array. Data is written to the current write $position pointer which is 
         *      then incremented.
         *  @param data Data to write
         */
        native function writeInteger(data: Number): Void

        /**
         *  Write a 64 bit long integer to the array. Data is written to the current write $position pointer which is 
         *  then incremented.
         *  @param data Data to write
         */
        native function writeLong(data: Number): Void

        /**
         *  Current write position offset.
         */
        native function get writePosition(): Number

        /**
         *  Set the current write position offset.
         *  @param position the new write  position
         */
        native function set writePosition(position: Number): Void
    }
}


/*
 *  @copy   default
 *  
 *  Copyright (c) Embedthis Software LLC, 2003-2010. All Rights Reserved.
 *  Copyright (c) Michael O'Brien, 1993-2010. All Rights Reserved.
 *  
 *  This software is distributed under commercial and open source licenses.
 *  You may use the GPL open source license described below or you may acquire 
 *  a commercial license from Embedthis Software. You agree to be fully bound 
 *  by the terms of either license. Consult the LICENSE.TXT distributed with 
 *  this software for full details.
 *  
 *  This software is open source; you can redistribute it and/or modify it 
 *  under the terms of the GNU General Public License as published by the 
 *  Free Software Foundation; either version 2 of the License, or (at your 
 *  option) any later version. See the GNU General Public License for more 
 *  details at: http://www.embedthis.com/downloads/gplLicense.html
 *  
 *  This program is distributed WITHOUT ANY WARRANTY; without even the 
 *  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
 *  
 *  This GPL license does NOT permit incorporating this software into 
 *  proprietary programs. If you are unable to comply with the GPL, you must
 *  acquire a commercial license to use this software. Commercial licenses 
 *  for this software and support services are available from Embedthis 
 *  Software at http://www.embedthis.com 
 *  
 *  @end
 */
/************************************************************************/
/*
 *  End of file "../src/es/core/ByteArray.es"
 */
/************************************************************************/



/************************************************************************/
/*
 *  Start of file "../src/es/core/Date.es"
 */
/************************************************************************/

/*
    Date.es -- Date class

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

module ejs {

    use default namespace intrinsic

    /**
        General purpose class for representing and working with dates, times, time spans and time zones.
        @stability evolving
     */
    native final class Date {

        use default namespace public

        /**
            Construct a new date object. Permissible constructor forms:
            <ul>
                <li>Date()</li>
                <li>Date(milliseconds) where (seconds sincde 1 Jan 1970 00:00:00 UTC))</li>
                <li>Date(dateString) where (In a format recognized by parse())</li>
                <li>Date(year, month, date) where (Four digit year, month: 0-11, date: 1-31)</li>
                <li>Date(year, month, date [, hour, minute, second, msec]) where (hour: 0-23, minute: 0-59, second: 0-59, msec: 0-999)</li>
            </ul>
         */
        native function Date(...args)

        /**
            The day of the week (0 - 6, where 0 is Sunday) in local time. 
            @spec ejs
         */
        native function get day(): Number 

        /**
            The day of the week (0 - 6, where 0 is Sunday) in local time. 
            @param day Day of the week
            @spec ejs
         */
        native function set day(day: Number): Void

        /**
            The day of the year (0 - 365) in local time.
            @spec ejs
         */
        native function get dayOfYear(): Number 

        /**
            The day of the year (0 - 365) in local time.
            @param day Day of the year
            @spec ejs
         */
        native function set dayOfYear(day: Number): Void

        /**
            The day of the month (1-31).
            @spec ejs
         */
        native function get date(): Number 

        /**
            The day of the month (1-31).
            @param date integer day of the month (1-31)
            @spec ejs
         */
        native function set date(date: Number): Void

        /**
            Time in milliseconds since the date object was constructed
            @spec ejs
         */
        native function get elapsed(): Number

        /**
            Format a date using a format specifier in local time. This routine is implemented by calling 
            the O/S strftime routine and so not all the format specifiers are available on all platforms.

            The format specifiers are:
            <ul>
            <li>%A    national representation of the full weekday name.</li>
            <li>%a    national representation of the abbreviated weekday name.</li>
            <li>%B    national representation of the full month name.</li>
            <li>%b    national representation of the abbreviated month name.</li>
            <li>%C    (year / 100) as decimal number; single digits are preceded by a zero.</li>
            <li>%c    national representation of time and date.</li>
            <li>%D    is equivalent to ``%m/%d/%y''.</li>
            <li>%d    the day of the month as a decimal number (01-31).</li>
            <li>%E*   POSIX locale extensions. The sequences %Ec %EC %Ex %EX %Ey %EY are supposed to provide alternate 
                      representations. NOTE: these are not available on some platforms.</li>
            <li>%e    the day of month as a decimal number (1-31); single digits are preceded by a blank.</li>
            <li>%F    is equivalent to ``%Y-%m-%d''.</li>
            <li>%G    a year as a decimal number with century. This year is the one that contains the greater part of
                      the week (Monday as the first day of the week).</li>
            <li>%g    the same year as in ``%G'', but as a decimal number without century (00-99).</li>
            <li>%H    the hour (24-hour clock) as a decimal number (00-23).</li>
            <li>%h    the same as %b.</li>
            <li>%I    the hour (12-hour clock) as a decimal number (01-12).</li>
            <li>%j    the day of the year as a decimal number (001-366). Note: this range is different to that of
                      the dayOfYear property which is 0-365.</li>
            <li>%k    the hour (24-hour clock) as a decimal number (0-23); single digits are preceded by a blank.</li>
            <li>%l    the hour (12-hour clock) as a decimal number (1-12); single digits are preceded by a blank.</li>
            <li>%M    the minute as a decimal number (00-59).</li>
            <li>%m    the month as a decimal number (01-12).</li>
            <li>%n    a newline.</li>
            <li>%O*   POSIX locale extensions. The sequences %Od %Oe %OH %OI %Om %OM %OS %Ou %OU %OV %Ow %OW %Oy are 
                      supposed to provide alternate representations. Additionly %OB implemented to represent alternative 
                      months names (used standalone, without day mentioned). NOTE: these are not available on some 
                      platforms.</li>
            <li>%P    Lower case national representation of either "ante meridiem" or "post meridiem" as appropriate.</li>
            <li>%p    national representation of either "ante meridiem" or "post meridiem" as appropriate.</li>
            <li>%R    is equivalent to ``%H:%M''.</li>
            <li>%r    is equivalent to ``%I:%M:%S %p''.</li>
            <li>%S    the second as a decimal number (00-60).</li>
            <li>%s    the number of seconds since the Epoch, UTC (see mktime(3)).</li>
            <li>%T    is equivalent to ``%H:%M:%S''.</li>
            <li>%t    a tab.</li>
            <li>%U    the week number of the year (Sunday as the first day of the week) as a decimal number (00-53).</li>
            <li>%u    the weekday (Monday as the first day of the week) as a decimal number (1-7).</li>
            <li>%V    the week number of the year (Monday as the first day of the week) as a decimal
                      number (01-53).  If the week containing January 1 has four or more days in the new year, then it
                      is week 1; otherwise it is the last week of the previous year, and the next week is week 1.</li>
            <li>%v    is equivalent to ``%e-%b-%Y''.</li>
            <li>%W    the week number of the year (Monday as the first day of the week) as a decimal number (00-53).</li>
            <li>%w    the weekday (Sunday as the first day of the week) as a decimal number (0-6).</li>
            <li>%X    national representation of the time.</li>
            <li>%x    national representation of the date.</li>
            <li>%Y    the year with century as a decimal number.</li>
            <li>%y    the year without century as a decimal number (00-99).</li>
            <li>%Z    the time zone name.</li>
            <li>%z    the time zone offset from UTC; a leading plus sign stands for east of UTC, a minus
                      sign for west of UTC, hours and minutes follow with two digits each and no delimiter between them
                      (common form for RFC 822 date headers).</li>
            <li>%+    national representation of the date and time (the format is similar to that produced by date(1)).
                      This format is platform dependent.</li>
            <li>%%    Literal percent.</li>
            </ul>
        
            @param layout Format layout string using the above format specifiers. Similar to a C language printf() string.
            @return string representation of the date.
            @spec ejs
         */
        native function format(layout: String): String 

        /**
            Format a date using a format specifier in UTC time. This routine is implemented by calling 
            the O/S strftime routine and so not all the format specifiers are available on all platforms.
            @param layout Format layout string using the above format specifiers. See format() for the list of format
            specifiers.
            @return string representation of the date.
            @spec ejs
         */
        native function formatUTC(layout: String): String 

        /**
            The year as four digits in local time.
            @spec ejs
         */
        native function get fullYear(): Number 

        /**
            Set the year as four digits in local time.
            @param year Year to set.
            @spec ejs
         */
        native function set fullYear(year: Number): void

        /**
            Return the day of the month in local time
            @return Returns the day of the year (1-31)
         */
        function getDate(): Number 
            date

        /**
            Return the day of the week in local time.
            @return The integer day of the week (0 - 6, where 0 is Sunday)
         */
        function getDay(): Number 
            day

        /**
            Return the year as four digits in local time
            @return The integer year
         */
        function getFullYear(): Number 
            fullYear

        /**
            Return the hour (0 - 23) in local time.
            @return The integer hour of the day
         */
        function getHours(): Number 
            hours

        /**
            Return the millisecond (0 - 999) in local time.
            @return The number of milliseconds as an integer
         */
        function getMilliseconds(): Number 
            milliseconds

        /**
            Return the minute (0 - 59) in local time.
            @return The number of minutes as an integer
         */
        function getMinutes(): Number 
            minutes

        /**
            Return the month (0 - 11) in local time.
            @return The month number as an integer
         */
        function getMonth(): Number 
            month

        /**
            Return the second (0 - 59) in local time.
            @return The number of seconds as an integer
         */
        function getSeconds(): Number 
            seconds

        /**
            Return the number of milliseconds since midnight, January 1st, 1970 UTC.
            @return The number of milliseconds as a long
         */
        function getTime(): Number
            time

        /**
            Return the number of minutes between the local computer time and Coordinated Universal Time.
            @return Integer containing the number of minutes between UTC and local time. The offset is positive if
            local time is behind UTC and negative if it is ahead. E.g. American PST is UTC-8 so 480 will be retured.
            This value will vary if daylight savings time is in effect.
         */
        native function getTimezoneOffset(): Number

        /**
            Return the day (date) of the month (1 - 31) in UTC time.
            @return The day of the month as an integer
         */
        native function getUTCDate(): Number 

        /**
            Return the day of the week (0 - 6) in UTC time.
            @return The day of the week as an integer
         */
        native function getUTCDay(): Number 

        /**
            Return the year in UTC time.
            @return The full year in 4 digits as an integer
         */
        native function getUTCFullYear(): Number 

        /**
            Return the hour (0 - 23) in UTC time.
            @return The integer hour of the day
         */
        native function getUTCHours(): Number 

        /**
            Return the millisecond (0 - 999) in UTC time.
            @return The number of milliseconds as an integer
         */
        native function getUTCMilliseconds(): Number 

        /**
            Return the minute (0 - 59) in UTC time.
            @return The number of minutes as an integer
         */
        native function getUTCMinutes(): Number 

        /**
            Return the month (1 - 12) in UTC time.
            @return The month number as an integer
         */
        native function getUTCMonth(): Number 

        /**
            Return the second (0 - 59) in UTC time.
            @return The number of seconds as an integer
         */
        native function getUTCSeconds(): Number 

        /**
            The current hour (0 - 23) in local time.
            @spec ejs
         */
        native function get hours(): Number 

        /**
            The current hour (0 - 23) in local time
            @param hour The hour as an integer
            @spec ejs
         */
        native function set hours(hour: Number): void

        /**
            The current millisecond (0 - 999) in local time.
            @return The number of milliseconds as an integer
            @spec ejs
         */
        native function get milliseconds(): Number 

        /**
            Set the current millisecond (0 - 999) in local time.
            @param ms The millisecond as an integer
            @spec ejs
         */
        native function set milliseconds(ms: Number): void

        /**
            The current minute (0 - 59) in local time.
            @return The number of minutes as an integer
            @spec ejs
         */
        native function get minutes(): Number 

        /**
            Set the current minute (0 - 59) in local time.
            @param min The minute as an integer
            @spec ejs
         */
        native function set minutes(min: Number): void

        /**
            The current month (0 - 11) in local time.
            @return The month number as an integer
            @spec ejs
         */
        native function get month(): Number 

        /**
            Set the current month (0 - 11) in local time.
            @param month The month as an integer
            @spec ejs
         */
        native function set month(month: Number): void

        /**
            Time in nanoseconds since the date object was constructed
            @spec ejs
         */
        function nanoAge(): Number
            elapsed * 1000

        /**
            Return a new Date object that is one day greater than this one.
            @param inc Increment in days to add (or subtract if negative)
            @return A Date object
            @spec ejs
         */
        native function nextDay(inc: Number = 1): Date

        /**
            Return the current time as milliseconds since Jan 1 1970.
            @spec mozilla
         */
        static native function now(): Number

        /**
            Parse a date string and Return a new Date object. If $dateString does not contain a timezone,
                the date string will be interpreted as a local date/time.  This is similar to parse() but it returns a
                date object.
            @param dateString The date string to parse.
            @param defaultDate Default date to use to fill out missing items in the date string.
            @return Return a new Date.
            @spec ejs
         */
        static native function parseDate(dateString: String, defaultDate: Date = null): Date

        /**
            Parse a date string and Return a new Date object. If $dateString does not contain a timezone,
                the date string will be interpreted as a UTC date/time.
            @param dateString UTC date string to parse.
            @param defaultDate Default date to use to fill out missing items in the date string.
            @return Return a new Date.
            @spec ejs
         */
        static native function parseUTCDate(dateString: String, defaultDate: Date = null): Date

        /**
            Parse a date string and return the number of milliseconds since midnight, January 1st, 1970 UTC. 
            If $dateString does not contain a timezone, the date string will be interpreted as a local date/time.
            @param dateString The string to parse
            @return Return a new date number.
         */
        static native function parse(dateString: String): Number

        /**
            The current second (0 - 59) in local time.
            @return The number of seconds as an integer
            @spec ejs
         */
        native function get seconds(): Number 

        /**
            Set the current second (0 - 59) in local time.
            @param sec The second as an integer
            @spec ejs
         */
        native function set seconds(sec: Number): void

        /**
            Set the date of the month (0 - 31)
            @param d Date of the month
         */
        function setDate(d: Number): void
            date = d

        /**
            Set the current year as four digits in local time.
            @param y Current year
         */
        function setFullYear(y: Number): void
            year = y

        /**
            Set the current hour (0 - 59) in local time.
            @param h The hour as an integer
         */
        function setHours(h: Number): void
            hours = h

        /**
            Set the current millisecond (0 - 999) in local time.
            @param ms The millisecond as an integer
         */
        function setMilliseconds(ms: Number): void
            milliseconds = ms

        /**
            Set the current minute (0 - 59) in local time.
            @param min The minute as an integer
         */
        function setMinutes(min: Number): void
            minutes = min

        /**
            Set the current month (0 - 11) in local time.
            @param mon The month as an integer
         */
        function setMonth(mon: Number): void
            month = mon

        /**
            Set the current second (0 - 59) in local time.
            @param sec The second as an integer
            @param msec Optional milliseconds as an integer
         */
        function setSeconds(sec: Number, msec: Number = null): void {
            seconds = sec
            if (msec != null) {
                milliseconds = msec
            }
        }

        /**
            Set the number of milliseconds since midnight, January 1st, 1970 UTC.
            @param value The millisecond as a long
         */
        function setTime(value: Number): void
            time = value

        /**
            Set the date of the month (0 - 31) in UTC time.
            @param d The date to set
         */
        native function setUTCDate(d: Number): void

        /**
            Set the current year as four digits in UTC time.
            @param y The year to set
         */
        native function setUTCFullYear(y: Number): void

        /**
            Set the current hour (0 - 59) in UTC time.
            @param h The hour as an integer
         */
        native function setUTCHours(h: Number): void

        /**
            Set the current millisecond (0 - 999) in UTC time.
            @param ms The millisecond as an integer
         */
        native function setUTCMilliseconds(ms: Number): void

        /**
            Set the current minute (0 - 59) in UTC time.
            @param min The minute as an integer
         */
        native function setUTCMinutes(min: Number): void

        /**
            Set the current month (0 - 11) in UTC time.
            @param mon The month as an integer
         */
        native function setUTCMonth(mon: Number): void

        /**
            Set the current second (0 - 59) in UTC time.
            @param sec The second as an integer
            @param msec Optional milliseconds as an integer
         */
        native function setUTCSeconds(sec: Number, msec: Number = null): void

        /**
            The number of milliseconds since midnight, January 1st, 1970 UTC and the current date object.
            This is the same as Date.now()
            @spec ejs
         */
        native function get time(): Number 

        /**
            Set the number of milliseconds since midnight, January 1st, 1970 UTC.
            @param value The number of milliseconds as a number
            @spec ejs
         */
        native function set time(value: Number): Void 

        /**
            Return a string containing the date portion excluding the time portion of the date in local time.
            The format is American English.
            Sample: "Fri Jan 15 2010"
            @return A string representing the date portion.
         */
        function toDateString(): String 
            format("%a %b %d %Y")

        /**
            Convert a date to an equivalent JSON encoding.
            @return This function returns a date in ISO format as a string.
            @throws TypeError If the object could not be converted to a string.
         */ 
        native override function toJSON(): String

        /**
            Return an ISO formatted date string.
            Sample: "2006-12-15T23:45:09.33-08:00"
            @return An ISO formatted string representing the date.
            @spec ejs
         */
        native function toISOString(): String 

        /**
            Return a localized string containing the date portion excluding the time portion of the date in local time.
            Note: You should not rely on the format of the output as the exact format will depend on the platform
            and locale.
            Sample: "Fri, 15 Dec 2006 GMT-0800". (Note: Other platforms render as:
            V8  format: "Fri, 15 Dec 2006 GMT-0800"
            JS  format: "01/15/2010"
            JSC format: "January 15, 2010")
            @return A string representing the date portion.
         */
        function toLocaleDateString(): String 
            format("%a, %d %b %Y GMT%z")

        /**
            Return a localized string containing the date. This formats the date using the operating system's locale
            conventions.
            Sample:  "Fri, 15 Dec 2006 23:45:09 GMT-0800 (PST)". (Note: Other platforms render as:
            V8 format:  "Fri, 15 Dec 2006 23:45:09 GMT-0800 (PST)"
            JS format:  "Fri Jan 15 13:09:02 2010"
            JSC format: "January 15, 2010 1:09:06 PM PST"
            @return A string representing the date.
         */
        function toLocaleString(): String
            format("%a, %d %b %Y %T GMT%z (%Z)")

        /**
            Return a string containing the time portion of the date in local time.
            Sample:  "13:08:57". Note: Other platforms render as:
            V8 format:  "13:08:57"
            JS format:  "13:09:02"
            JSC format: "1:09:06 PM PST"
            @return A string representing the time.
         */
        function toLocaleTimeString(): String 
            format("%X")

        /**
            Return a string representing the date in local time. The format is American English.
            Sample: "Fri, 15 Dec 2006 23:45:09 GMT-0800 (PST)"
            @return A string representing the date.
         */
        override native function toString(): String 

        /**
            Return a string containing the time portion in human readable form in American English.
            Sample: "13:08:57 GMT-0800 (PST)"
            @return A string representing the time.
         */
        function toTimeString(): String 
            format("%X GMT%z (%Z)")

        /**
            Return a string containing the date in UTC time.
            Sample: "Sat, 16 Dec 2006 08:06:21 GMT"
            @return A string representing the date.
         */
        function toUTCString(): String 
            formatUTC("%a, %d %b %Y %T GMT")

        /**
            Construct a new date object interpreting its arguments in UTC rather than local time.
            Date(year, month, date [, hour, minute, second, msec])</li>
            @param year Year
            @param month Month of year
            @param day Day of month
            @param hours Hour of day
            @param minutes Minute of hour
            @param seconds Secods of minute
            @param milliseconds Milliseconds of second
         */
        native static function UTC(year: Number, month: Number, day: Number, hours: Number = 0, 
                minutes: Number = 0, seconds: Number = 0, milliseconds: Number = 0): Date

        /**
            Return the primitive value of the object
            @returns A number corresponding to the $time property.
         */ 
        function valueOf(): Number
            time

        /**
            The current year as two digits.
            @spec ejs
         */
        native function get year(): Number 

        /**
            Set the current year as two digits in local time.
            @param year Year to set.
            @spec ejs
         */
        native function set year(year: Number): void

        /**
            Date difference. Return a new Date that is the difference of the two dates.
            @param The operand date
            @return Return a new Date.
         */
        # TODO
        native function -(date: Date): Date
    }
}


/*
    @copy   default
    
    Copyright (c) Embedthis Software LLC, 2003-2010. All Rights Reserved.
    Copyright (c) Michael O'Brien, 1993-2010. All Rights Reserved.
    
    This software is distributed under commercial and open source licenses.
    You may use the GPL open source license described below or you may acquire 
    a commercial license from Embedthis Software. You agree to be fully bound 
    by the terms of either license. Consult the LICENSE.TXT distributed with 
    this software for full details.
    
    This software is open source; you can redistribute it and/or modify it 
    under the terms of the GNU General Public License as published by the 
    Free Software Foundation; either version 2 of the License, or (at your 
    option) any later version. See the GNU General Public License for more 
    details at: http://www.embedthis.com/downloads/gplLicense.html
    
    This program is distributed WITHOUT ANY WARRANTY; without even the 
    implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
    
    This GPL license does NOT permit incorporating this software into 
    proprietary programs. If you are unable to comply with the GPL, you must
    acquire a commercial license to use this software. Commercial licenses 
    for this software and support services are available from Embedthis 
    Software at http://www.embedthis.com 
    
    @end
 */
/************************************************************************/
/*
 *  End of file "../src/es/core/Date.es"
 */
/************************************************************************/



/************************************************************************/
/*
 *  Start of file "../src/es/core/Error.es"
 */
/************************************************************************/

/*
 *  Error.es -- Error exception classes
 *
 *  Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */

module ejs {

    use default namespace intrinsic

    /**
     *  Arguments error exception class. 
     *  Thrown the arguments cannot be cast to the required type or
     *  in strict mode if there are too few or too many arguments.
     *  @spec ejs
        @stability evolving
     */
    native dynamic class ArgError extends Error {
        /**
         *  ArgError constructor.
         *  @params message Message to use when defining the Error.message property
         */
        native function ArgError(message: String = null) 
    }

    /**
     *  Arithmetic error exception class. Thrown when the system cannot perform an arithmetic operation, 
     *  e.g. on divide by zero.
     *  @spec ejs
     *  @stability evolving
     */
    native dynamic class ArithmeticError extends Error {
        /**
         *  ArithmeticError constructor.
         *  @params message Message to use when defining the Error.message property
         */
        native function ArithmeticError(message: String = null) 
    }

    /**
     *  Assertion error exception class. Thrown when the $assert method is invoked with a false value.
     *  @spec ejs
     *  @stability evolving
     */
    native dynamic class AssertError extends Error {
        /**
         *  AssertError constructor.
         *  @params message Message to use when defining the Error.message property
         */
        native function AssertError(message: String = null) 
    }

    /**
     *  Code (instruction) error exception class. Thrown when an illegal or insecure operation code is detected 
     *  in the instruction stream.
     *  @spec ejs
     *  @stability evolving
     */
    native dynamic class InstructionError extends Error {
        /**
         *  InstructionError constructor.
         *  @params message Message to use when defining the Error.message property
         */
        native function InstructionError(message: String = null) 
    }

    /**
     *  Base class for error exception objects. Exception objects are created by the system as part of changing 
     *  the normal flow of execution when some error condition occurs. 
     *
     *  When an exception is created and acted upon ("thrown"), the system transfers the flow of control to a 
     *  pre-defined instruction stream (the handler or "catch" code). The handler may return processing to the 
     *  point at which the exception was thrown or not. It may re-throw the exception or pass control up the call stack.
     */
    native dynamic class Error {

        use default namespace public

        /**
         *  Exception error message.
         */
        native var message: String

        /**
         *  Optional error code
         */
        native function get code(): Number

        /**
         *  Set an optional error code
         *  @param value Error code to set
         */
        native function set code(value: Number): Void

        /**
         *  Execution stack backtrace. Contains the execution stack backtrace at the time the exception was thrown.  
         */
        native var stack: String 

        /**
         *  Construct a new Error object.
         *  @params message Message to use when defining the Error.message property
         */
        native function Error(message: String = null)
    }

    /**
     *  IO error exception class. Thrown when an I/O/ interruption or failure occurs, e.g. a file is not found 
     *  or there is an error in a communication stack.
     *  @spec ejs
     *  @stability evolving
     */
    native dynamic class IOError extends Error {

        /**
         *  IOError constructor.
         *  @params message Message to use when defining the Error.message property
         */
        native function IOError(message: String = null) 
    }

    /**
     *  Internal error exception class. Thrown when some error occurs in the virtual machine.
     *  @spec ejs
     *  @stability evolving
     *  @hide
     */
    native dynamic class InternalError extends Error {
        /**
         *  InternalError constructor.
         *  @params message Message to use when defining the Error.message property
         */
        native function InternalError(message: String = null) 
    }

    /**
     *  Memory error exception class. Thrown when the system attempts to allocate memory and none is available 
     *  or the stack overflows.
     *  @spec ejs
     *  @stability evolving
     */
    native dynamic class MemoryError extends Error {
        /**
         *  MemoryError constructor.
         *  @params message Message to use when defining the Error.message property
         */
        native function MemoryError(message: String = null) 
    }

    /**
     *  OutOfBounds error exception class. Thrown to indicate that an attempt has been made to set or access an 
     *  object's property outside of the permitted set of values for that property. For example, an array has been 
     *  accessed with an illegal index or, in a date object, attempting to set the day of the week to greater then 7.
     *  @spec ejs
     *  @stability evolving
     *  @hide
     */
    native dynamic class OutOfBoundsError extends Error {

        /**
         *  OutOfBoundsError constructor.
         *  @params message Message to use when defining the Error.message property
         */
        native function OutOfBoundsError(message: String = null) 
    }

    /**
     *  Reference error exception class. Thrown when an invalid reference to an object is made, e.g. a method is 
     *  invoked on an object whose type does not define that method.
     *  @stability evolving
     */
    native dynamic class ReferenceError extends Error {

        /**
         *  ReferenceError constructor.
         *  @params message Message to use when defining the Error.message property
         */
        native function ReferenceError(message: String = null)
    }

    /**
     *  Resource error exception class. Thrown when the system cannot allocate a resource it needs to continue, 
     *  e.g. a native thread, process, file handle or the like.
     *  @spec ejs
     *  @stability evolving
     */
    native dynamic class ResourceError extends Error {

        /**
         *  ResourceError constructor.
         *  @params message Message to use when defining the Error.message property
         */
        native function ResourceError(message: String = null) 
    }

    /**
     *  Security error exception class. Thrown when an access violation occurs. Access violations include attempting 
     *  to write a file without having write permission or assigning permissions without being the owner of the 
     *  securable entity.
     *  @spec ejs
     *  @stability evolving
     *  @hide
     */
    # FUTURE
    native dynamic class SecurityError extends Error {
        /**
         *  SecurityError constructor.
         *  @params message Message to use when defining the Error.message property
         */
        native function SecurityError(message: String = null) 
    }

    /**
     *  State error exception class. Thrown when an object cannot be transitioned from its current state to the 
     *  desired state, e.g. calling "sleep" on an interrupted thread.
     *  @spec ejs
     *  @stability evolving
     */
    native dynamic class StateError extends Error {
        /**
         *  StateError constructor.
         *  @params message Message to use when defining the Error.message property
         */
        native function StateError(message: String = null) 
    }

    /**
     *  Syntax error exception class. Thrown when the system cannot parse a character sequence for the intended 
     *  purpose, e.g. a regular expression containing invalid characters.
     *  @stability evolving
     */
    native dynamic class SyntaxError extends Error {
        /**
         *  SyntaxError constructor.
         *  @params message Message to use when defining the Error.message property
         */
        native function SyntaxError(message: String = null) 
    }

    /**
     *  Type error exception class. Thrown when a type casting or creation operation fails, e.g. when an operand 
     *  cannot be cast to a type that allows completion of a statement or when a type cannot be found for object 
     *  creation or when an object cannot be instantiated given the values passed into "new".
     *  @stability evolving
     */
    native dynamic class TypeError extends Error {

        /**
         *  TypeError constructor.
         *  @params message Message to use when defining the Error.message property
         */
        native function TypeError(message: String = null) 
    }

    /**
     *  URI error exception class. Thrown a URI fails to parse.
     *  @stability prototype
     *  @hide
     */
    native dynamic class URIError extends Error {
        /**
         *  URIError constructor.
         *  @params message Message to use when defining the Error.message property
         */
        native function URIError(message: String = null) 
    }
}


/*
 *  @copy   default
 *  
 *  Copyright (c) Embedthis Software LLC, 2010-2010. All Rights Reserved.
 *  Copyright (c) Michael O'Brien, 2010-2010. All Rights Reserved.
 *  
 *  This software is distributed under commercial and open source licenses.
 *  You may use the GPL open source license described below or you may acquire 
 *  a commercial license from Embedthis Software. You agree to be fully bound 
 *  by the terms of either license. Consult the LICENSE.TXT distributed with 
 *  this software for full details.
 *  
 *  This software is open source; you can redistribute it and/or modify it 
 *  under the terms of the GNU General Public License as published by the 
 *  Free Software Foundation; either version 2 of the License, or (at your 
 *  option) any later version. See the GNU General Public License for more 
 *  details at: http://www.embedthis.com/downloads/gplLicense.html
 *  
 *  This program is distributed WITHOUT ANY WARRANTY; without even the 
 *  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
 *  
 *  This GPL license does NOT permit incorporating this software into 
 *  proprietary programs. If you are unable to comply with the GPL, you must
 *  acquire a commercial license to use this software. Commercial licenses 
 *  for this software and support services are available from Embedthis 
 *  Software at http://www.embedthis.com 
 *  
 *  @end
 *
 */
/************************************************************************/
/*
 *  End of file "../src/es/core/Error.es"
 */
/************************************************************************/



/************************************************************************/
/*
 *  Start of file "../src/es/core/Frame.es"
 */
/************************************************************************/

/*
 *  Frame.es -- Frame class
 *
 *  Copyright (c) All Rights Reserved. See details at the end of the file.
 */

module ejs {

    use default namespace intrinsic

    /**
        Reserved slot marker for Frame activation records. Used by the GC to manage the activation record pool.
        @hide
     */
    var Frame
}

/*
 *  @copy   default
 *  
 *  Copyright (c) Embedthis Software LLC, 2003-2010. All Rights Reserved.
 *  Copyright (c) Michael O'Brien, 1993-2010. All Rights Reserved.
 *  
 *  This software is distributed under commercial and open source licenses.
 *  You may use the GPL open source license described below or you may acquire 
 *  a commercial license from Embedthis Software. You agree to be fully bound 
 *  by the terms of either license. Consult the LICENSE.TXT distributed with 
 *  this software for full details.
 *  
 *  This software is open source; you can redistribute it and/or modify it 
 *  under the terms of the GNU General Public License as published by the 
 *  Free Software Foundation; either version 2 of the License, or (at your 
 *  option) any later version. See the GNU General Public License for more 
 *  details at: http://www.embedthis.com/downloads/gplLicense.html
 *  
 *  This program is distributed WITHOUT ANY WARRANTY; without even the 
 *  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
 *  
 *  This GPL license does NOT permit incorporating this software into 
 *  proprietary programs. If you are unable to comply with the GPL, you must
 *  acquire a commercial license to use this software. Commercial licenses 
 *  for this software and support services are available from Embedthis 
 *  Software at http://www.embedthis.com 
 *  
 *  @end
 */
/************************************************************************/
/*
 *  End of file "../src/es/core/Frame.es"
 */
/************************************************************************/



/************************************************************************/
/*
 *  Start of file "../src/es/core/Function.es"
 */
/************************************************************************/

/*
 *  Function.es -- Function class
 *
 *  Copyright (c) All Rights Reserved. See details at the end of the file.
 */

module ejs {

    use default namespace intrinsic

    /**
        The Function type is used to represent closures, function expressions and class methods. It contains a 
        reference to the code to execute, the execution scope and possibly a bound "this" reference.
        @stability evolving
     */
    native final class Function {

        use default namespace public

        /**
         *  Invoke the function on another object.
         *  @param thisObject The object to set as the "this" object when the function is called.
         *  @param args Array of actual parameters to the function.
         *  @return Any object returned as a result of applying the function
         *  @throws ReferenceError If the function cannot be applied to this object.
         */
        native function apply(thisObject: Object, args: Array): Object 

        /**
         *  Invoke the function on another object. This function takes the "this" parameter and then a variable 
         *      number of actual parameters to pass to the function.
         *  @param thisObject The object to set as the "this" object when the function is called.
         *  @param args Actual parameters to the function.
         *  @return Any object returned as a result of applying the function
         *  @throws ReferenceError If the function cannot be applied to this object.
         */
        native function call(thisObject: Object, ...args): Object 

        /**
         *  Return the bound object representing the "this" object. Functions carry both a lexical scoping and 
         *  the owning "this" object.
         *  @return An object
         */
        # FUTURE
        native function get boundThis(): Object
    }

    /** @hide */
    native function makeGetter(fn: Function): Function

    /** @hide */
    native function clearBoundThis(fn: Function): Function
}

/*
 *  @copy   default
 *  
 *  Copyright (c) Embedthis Software LLC, 2003-2010. All Rights Reserved.
 *  Copyright (c) Michael O'Brien, 1993-2010. All Rights Reserved.
 *  
 *  This software is distributed under commercial and open source licenses.
 *  You may use the GPL open source license described below or you may acquire 
 *  a commercial license from Embedthis Software. You agree to be fully bound 
 *  by the terms of either license. Consult the LICENSE.TXT distributed with 
 *  this software for full details.
 *  
 *  This software is open source; you can redistribute it and/or modify it 
 *  under the terms of the GNU General Public License as published by the 
 *  Free Software Foundation; either version 2 of the License, or (at your 
 *  option) any later version. See the GNU General Public License for more 
 *  details at: http://www.embedthis.com/downloads/gplLicense.html
 *  
 *  This program is distributed WITHOUT ANY WARRANTY; without even the 
 *  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
 *  
 *  This GPL license does NOT permit incorporating this software into 
 *  proprietary programs. If you are unable to comply with the GPL, you must
 *  acquire a commercial license to use this software. Commercial licenses 
 *  for this software and support services are available from Embedthis 
 *  Software at http://www.embedthis.com 
 *  
 *  @end
 */
/************************************************************************/
/*
 *  End of file "../src/es/core/Function.es"
 */
/************************************************************************/



/************************************************************************/
/*
 *  Start of file "../src/es/core/Global.es"
 */
/************************************************************************/

/*
 *  Global.es -- Global variables, namespaces and functions.
 *
 *  Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/*
 *  Notes:
 *      native means supplied as a builtin (native C/Java)
 *      intrinsic implies ReadOnly, DontDelete, DontEnum
 */

module ejs {


    /** @hide */
    public var ECMA: Boolean = false

    /**
     *  The public namespace used to make entities visible accross modules.
     */
    public namespace public

    /**
     *  The internal namespace used to make entities visible within a single module only.
     */
    public namespace internal

    /**
     *  The intrinsic namespace used for entities that are part of and intrinsic to, the Ejscript platform.
     */
    public namespace intrinsic

    /**
     *  The iterator namespace used to defined iterators.
     */
    public namespace iterator

    /**
     *  The CONFIG namespace used to defined conditional compilation directives.
     */
    public namespace CONFIG

    use default namespace intrinsic
    use namespace iterator

    use namespace "ejs.sys"

    /** 
        Conditional compilation constant. Used to disable compilation of certain elements.
        @hide
     */  
    const TODO: Boolean = false

    /** 
     *  Conditional compilation constant. Used to disable compilation of certain elements.
     */  
    const FUTURE: Boolean = false

    /** 
     *  Conditional compilation constant. Used to disable compilation of certain elements.
     */  
    const ASC: Boolean = false

    /** 
     *  Conditional compilation constant. Used to enable the compilation of elements only for creating the API documentation.
     */  
    const DOC_ONLY: Boolean = false

    /** 
     *  Conditional compilation constant. Used to deprecate elements.
     */  
    const DEPRECATED: Boolean = false

    /** 
     *  Conditional compilation constant. Used to deprecate elements.
     */  
    const REGEXP: Boolean = true

    /**
     *  Alias for the Boolean type
     */
    native const boolean: Type = Boolean

    /**
     *  Alias for the Number type
     */
    native const double: Type = Number

    /**
     *  Alias for the Number type
     *  @spec ejs
     */
    native const num: Type = Number

    /**
     *  Alias for the String type
     */
    native const string: Type = String

    /**
     *  Boolean false value.
     */
    native const false: Boolean

    /**
     *  Global variable space reference. The global variable references an object which is the global variable space. 
     *  This is useful when guaranteed access to a global variable is required. e.g. global.someName.
     */
    native var global: Object

    /**
     *  Null value. The null value is returned when testing a nullable variable that has not yet had a 
     *  value assigned or one that has had null explicitly assigned.
     */
    native const null: Null

    /**
     *  Infinity value.
     */
    native const Infinity: Number

    /**
     *  Negative infinity value.
     */
    native const NegativeInfinity: Number

    /**
     *  Invalid numeric value. If the numeric type is set to an integral type, the value is zero.
     */
    native const NaN: Number

    /**
     *  StopIteration class. Iterators throw the StopIteration class instance to signal the end of iteration in for/in loops.
     *  @spec ejs
     */
    iterator native final class StopIteration {}

    /**
     *  True value.
     */
    native const true: Boolean

    /**
     *  Undefined variable value. The undefined value is returned when testing
     *  for a property that has not been defined. 
     */
    native const undefined: Void

    /**
     *  void type value. 
     *  This is an alias for Void.
     *  @spec ejs
     */
    native const void: Type = Void

    /**
     *  Assert a condition is true. This call tests if a condition is true by testing to see if the supplied 
     *  expression is true. If the expression is false, the interpreter will throw an exception.
     *  @param condition JavaScript expression evaluating or castable to a Boolean result.
     *  @return True if the condition is.
     *  @spec ejs
     */
    native function assert(condition: Boolean): Boolean

    /**
     *  Convenient way to trap to the debugger
     */
    native function breakpoint(): Void

    /**
     *  Replace the base type of a type with an exact clone. 
     *  @param klass Class in which to replace the base class.
     *  @spec ejs
     */
    native function cloneBase(klass: Type): Void

    /**
     *  Convert a string into an object. This will parse a string that has been encoded via serialize. It may contain
     *  nested objects and arrays. This is a static method.
     *  @param obj The string containing the object encoding.
     *  @return The fully constructed object or undefined if it could not be reconstructed.
     *  @throws IOError If the object could not be reconstructed from the string.
     *  @spec ejs
     */
    native function deserialize(obj: String): Object

    /**
     *  Reverse www-url encoding on a string
     *  @param str URL encoded string
     *  @returns a decoded string
     */
    native function decodeURI(str: String): String

    /**
     *  Dump the contents of objects. Used for debugging, this routine serializes the objects and prints to the standard
     *  output.
     *  @param args Variable number of arguments of any type
     */
    function dump(...args): Void {
        for each (var e: Object in args) {
            print(serialize(e))
        }
    }

    /**
     *  Write to the standard error. This call writes the arguments to the standard error with a new line appended. 
     *  It evaluates the arguments, converts the result to strings and prints the result to the standard error. 
     *  Arguments are converted to strings by calling their toSource method.
     *  @param args Data to write
     *  @spec ejs
     */
    native function error(...args): void

    /**
     *  HTML escape a string. This quotes characters which would otherwise be interpreted as HTML directives.
     *  @param str String to html escape
     *  @returns a HTML escaped string
     *  @spec ejs
     */
    native function escape(str: String): String

    /**
     *  Encode a string using  www-url encoding
     *  @param str URL encoded string
     *  @returns an encoded string
     *  @spec ejs
     */
    native function encodeURI(str: String): String

    /**
     *  Computed an MD5 sum of a string
     *  This function is prototype and may be renamed in a future release.
     *  @param str String on which to compute a checksum
     *  @returns An MD5 checksum
     *  @spec ejs
     */
    native function md5(str: String): String

    /**
     *  Evaluate a script. Not present in ejsvm.
     *  @param script Script to evaluate
     *  @returns the the script expression value.
     */
    native function eval(script: String): Object

    /**
     *  Format the current call stack. Used for debugging and when creating exception objects.
     *  @spec ejs
     */
    native function formatStack(): String

    /**
     *  Get the object's Unique hash id. All objects have a unique object hash. 
     *  @return This property accessor returns a long containing the object's unique hash identifier. 
     */ 
    native function hashcode(o: Object): Number

    /**
     *  Read from the standard input. This call reads a line of input from the standard input
     *  @return A string containing the input. Returns null on EOF.
     */
    native function input(): String

    /**
     *  Load a script or module
     *  @param file path name to load. File will be interpreted relative to EJSPATH if it is not found as an absolute or
     *      relative file name.
     */
    native function load(file: String): Void

    /**
     *  Print the arguments to the standard output with a new line appended. This call evaluates the arguments, 
     *  converts the result to strings and prints the result to the standard output. Arguments are converted to 
     *  strings by calling their toString method.
     *  @param args Variables to print
     *  @spec ejs
     */
    native function output(...args): void

    /**
     *  Print the arguments to the standard output with a new line appended. This call evaluates the arguments, 
     *  converts the result to strings and prints the result to the standard output. Arguments are converted to 
     *  strings by calling their toString method. This method invokes $output as its implementation. 
     *  @param args Variables to print
     *  @spec ejs
     */
    native function print(...args): void

    /**
     *  Print variables for debugging.
     *  @param args Variables to print
     *  @spec ejs
     */
    native function printv(...args): void

    /**
        Parse a string and convert to a primitive type
        @param str String to parse
        @param preferredType Preferred type to use if the input can be parsed multiple ways.
     */
    native function parse(str: String, preferredType: Type = null): Object

    /**
     *  Encode an object as a string. This function returns a literal string for the object and all its properties. 
     *  If @maxDepth is sufficiently large (or zero for infinite depth), each property will be processed recursively 
     *  until all properties are rendered.  NOTE: the maxDepth, all and base properties are not yet supported.
     *  @param obj Object to serialize
     *  @param maxDepth The depth to recurse when converting properties to literals. If set to zero, the depth is infinite.
     *  @param all Encode non-enumerable and class fixture properties and functions.
     *  @param base Encode base class properties.
     *  @return This function returns an object literal that can be used to reinstantiate an object.
     *  @throws TypeError If the object could not be converted to a string.
     *  @spec ejs
     */ 
    native function serialize(obj: Object, maxDepth: Number = 0, all: Boolean = false, base: Boolean = false): String

    /** @hide  temp only */
    function printHash(name: String, o: Object): Void
        print("%20s %X" % [name, hashcode(o)])

    /** @hide  */
    function instanceOf(obj: Object, target: Object): Boolean
        obj is target
}

/*
    Added to ease forward compatibility
 */
module ejs.unix {
}


/*
 *  @copy   default
 *  
 *  Copyright (c) Embedthis Software LLC, 2003-2010. All Rights Reserved.
 *  Copyright (c) Michael O'Brien, 1993-2010. All Rights Reserved.
 *  
 *  This software is distributed under commercial and open source licenses.
 *  You may use the GPL open source license described below or you may acquire 
 *  a commercial license from Embedthis Software. You agree to be fully bound 
 *  by the terms of either license. Consult the LICENSE.TXT distributed with 
 *  this software for full details.
 *  
 *  This software is open source; you can redistribute it and/or modify it 
 *  under the terms of the GNU General Public License as published by the 
 *  Free Software Foundation; either version 2 of the License, or (at your 
 *  option) any later version. See the GNU General Public License for more 
 *  details at: http://www.embedthis.com/downloads/gplLicense.html
 *  
 *  This program is distributed WITHOUT ANY WARRANTY; without even the 
 *  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
 *  
 *  This GPL license does NOT permit incorporating this software into 
 *  proprietary programs. If you are unable to comply with the GPL, you must
 *  acquire a commercial license to use this software. Commercial licenses 
 *  for this software and support services are available from Embedthis 
 *  Software at http://www.embedthis.com 
 *  
 *  @end
 */
/************************************************************************/
/*
 *  End of file "../src/es/core/Global.es"
 */
/************************************************************************/



/************************************************************************/
/*
 *  Start of file "../src/es/core/Iterator.es"
 */
/************************************************************************/

/**
 *  Iterator.es -- Iteration support via the Iterable interface and Iterator class. 
 *
 *  This provides a high performance native iterator for native classes. 
 *
 *  Copyright (c) All Rights Reserved. See details at the end of the file.
 */

module ejs {

    /**
     *  Iterable is an internal interface used by native types to provide iterators for use in for/in statements.
        @hide
        @stability evolving
     */
    iterator interface Iterable {
        use default namespace iterator

        /**
         *  Get an Iterator for use with for/in
         *  @return An Iterator
         */
        function get(): Iterator

        /**
         *  Get an Iterator for use with for each in
         *  @return An Iterator
         */
        function getValues(): Iterator
    }

    /**
     *  Iterator is a helper class to implement iterators.
     *  @hide
     */
    iterator native final class Iterator {

        use default namespace public

        /**
         *  Return the next element in the object.
         *  @return An object
         *  @throws StopIteration
         */
        native function next(): Object
    }
}


/*
 *  @copy   default
 *  
 *  Copyright (c) Embedthis Software LLC, 2003-2010. All Rights Reserved.
 *  Copyright (c) Michael O'Brien, 1993-2010. All Rights Reserved.
 *  
 *  This software is distributed under commercial and open source licenses.
 *  You may use the GPL open source license described below or you may acquire 
 *  a commercial license from Embedthis Software. You agree to be fully bound 
 *  by the terms of either license. Consult the LICENSE.TXT distributed with 
 *  this software for full details.
 *  
 *  This software is open source; you can redistribute it and/or modify it 
 *  under the terms of the GNU General Public License as published by the 
 *  Free Software Foundation; either version 2 of the License, or (at your 
 *  option) any later version. See the GNU General Public License for more 
 *  details at: http://www.embedthis.com/downloads/gplLicense.html
 *  
 *  This program is distributed WITHOUT ANY WARRANTY; without even the 
 *  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
 *  
 *  This GPL license does NOT permit incorporating this software into 
 *  proprietary programs. If you are unable to comply with the GPL, you must
 *  acquire a commercial license to use this software. Commercial licenses 
 *  for this software and support services are available from Embedthis 
 *  Software at http://www.embedthis.com 
 *  
 *  @end
 */
/************************************************************************/
/*
 *  End of file "../src/es/core/Iterator.es"
 */
/************************************************************************/



/************************************************************************/
/*
 *  Start of file "../src/es/core/JSON.es"
 */
/************************************************************************/

/*
 *  JSON.es -- JSON class
 *
 *  Copyright (c) All Rights Reserved. See details at the end of the file.
 */

module ejs {

    use default namespace intrinsic

    /**
     *  JavaScript Object Notation. This class supports the JSON data exchange format as described by:
     *  RFC 4627 at (http://www.ietf.org/rfc/rfc4627.txt).
     *  @stability evolving
     */
    final class JSON {

        use default namespace public

        /**
         *  Parse a string JSON representation and return an object equivalent
         *  @param data JSON string data to parse
         *  @param filter The optional filter parameter is a function that can filter and transform the results. It 
         *      receives each of the keys and values, and its return value is used instead of the original value. If 
         *      it returns what it received, then the structure is not modified. If it returns undefined then the 
         *      member is deleted. NOTE: the filter function is not yet implemented.
         *  @return An object representing the JSON string.
         */
        static function parse(data: String, filter: Function = null): Object
            deserialize(data)

        /**
         *  Convert an object into a string JSON representation
         *  @param obj Object to stringify
         *  @param replacer an optional parameter that determines how object values are stringified for objects without a 
         *      toJSON method. It can be a function or an array. NOTE: Not implemented.
         *  @param indent an optional parameter that specifies the indentation of nested structures. If it is omitted, 
         *      the text will be packed without extra whitespace. If it is a number, it will specify the number of spaces 
         *      to indent at each level. If it is a string (such as '\t' or '&nbsp;'), it contains the characters used to 
         *      indent at each level. NOTE: Not implemented.
         *  @return A JSON string representing the object
         */
        static function stringify(obj: Object, replacer: Object = null, indent: Number = 0): String
            serialize(obj)
    }
}


/*
 *  @copy   default
 *  
 *  Copyright (c) Embedthis Software LLC, 2003-2010. All Rights Reserved.
 *  Copyright (c) Michael O'Brien, 1993-2010. All Rights Reserved.
 *  
 *  This software is distributed under commercial and open source licenses.
 *  You may use the GPL open source license described below or you may acquire 
 *  a commercial license from Embedthis Software. You agree to be fully bound 
 *  by the terms of either license. Consult the LICENSE.TXT distributed with 
 *  this software for full details.
 *  
 *  This software is open source; you can redistribute it and/or modify it 
 *  under the terms of the GNU General Public License as published by the 
 *  Free Software Foundation; either version 2 of the License, or (at your 
 *  option) any later version. See the GNU General Public License for more 
 *  details at: http://www.embedthis.com/downloads/gplLicense.html
 *  
 *  This program is distributed WITHOUT ANY WARRANTY; without even the 
 *  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
 *  
 *  This GPL license does NOT permit incorporating this software into 
 *  proprietary programs. If you are unable to comply with the GPL, you must
 *  acquire a commercial license to use this software. Commercial licenses 
 *  for this software and support services are available from Embedthis 
 *  Software at http://www.embedthis.com 
 *  
 *  @end
 */
/************************************************************************/
/*
 *  End of file "../src/es/core/JSON.es"
 */
/************************************************************************/



/************************************************************************/
/*
 *  Start of file "../src/es/core/Math.es"
 */
/************************************************************************/

/*
    Math.es -- Math class 

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

module ejs {

    use default namespace intrinsic

    /**
        The Math class provides a set of static methods for performing common arithmetic, exponential and 
        trigonometric functions. It also provides commonly used constants such as PI. See also the Number class.
        Depending on the method and the supplied argument, return values may be real numbers, NaN (not a number) 
        or positive or negative infinity.
        @stability evolving
     */
    class Math extends Object {

        use default namespace public

        /**
            Base of natural logarithms (Euler's number).
         */
        static const E: Number = 2.7182818284590452354

        /**
            Natural log of 2.
         */
        static const LN2: Number = 0.6931471805599453

        /**
            Natural log of 10.
         */
        static const LN10: Number = 2.302585092994046

        /**
            Base 2 log of e.
         */
        static const LOG2E: Number = 1.4426950408889634

        /**
            Base 10 log of e.
         */
        static const LOG10E: Number = 0.4342944819032518

        /**
            The ratio of the circumference to the diameter of a circle.
         */
        static const PI: Number = 3.1415926535897932

        /**
            Reciprocal of the square root of 2.
         */
        static const SQRT1_2: Number = 0.7071067811865476

        /**
            Square root of 2.
         */
        static const SQRT2: Number = 1.4142135623730951

        /**
            Returns the absolute value of a number (which is equal to its magnitude).
            @param value Number value to examine
            @return the absolute value.
         */
        native static function abs(value: Number): Number 

        /**
            Calculates the arc cosine of an angle (in radians).
            @param angle In radians 
            @return The arc cosine of the argument 
         */
        native static function acos(angle: Number): Number 

        /**
            Calculates the arc sine of an angle (in radians).
            @param oper The operand.
            @return The arc sine of the argument 
         */
        native static function asin(oper: Number): Number 

        /**
            Calculates the arc tangent of an angle (in radians).
            @param oper The operand.
            @return The arc tanget of the argument 
         */
        native static function atan(oper: Number): Number 

        /**
            Calculates the arc tangent of the quotient of its arguments
            @param x the x operand.
            @param y the y operand.
            @return The arc tanget of the argument 
         */
        native static function atan2(y: Number, x: Number): Number 

        /**
            Return the smallest integer greater then this number.
            @return The ceiling
         */
        native static function ceil(oper: Number): Number

        /**
            Calculates the cosine of an angle (in radians).
            @param angle In radians 
            @return The cosine of the argument 
         */
        native static function cos(angle: Number): Number 
        
        /**
            Calculate E to the power of the argument
         */
        native static function exp(power: Number): Number 

        /**
            Returns the largest integer smaller then the argument.
            @param oper The operand.
            @return The floor
         */
        native static function floor(oper: Number): Number

        /**
            Calculates the log (base 10) of a number.
            @param oper The operand.
            @return The natural log of the argument
            @return The base 10 log of the argument
            @spec ejs
         */
        native static function log10(oper: Number): Number 
        
        /**
            Calculates the natural log (ln) of a number.
            @param oper The operand.
            @return The natural log of the argument
         */
        native static function log(oper: Number): Number 

        /**
            Returns the greater of the number or the argument.
            @param x First number to compare
            @param y Second number to compare
            @return A number
         */
        native static function max(x: Number, y: Number): Number

        /**
            Returns the lessor of the number or the argument.
            @param x First number to compare
            @param y Second number to compare
            @return A number
         */
        native static function min(x: Number, y: Number): Number

        /**
            Returns a number which is equal to this number raised to the power of the argument.
            @param num The number to raise to the power
            @param pow The exponent to raise @num to
            @return A number
         */
        native static function pow(num: Number, pow: Number): Number

        /**
            Generates a random number (a Number) inclusively between 0.0 and 1.0.
            @return A random number
         */
        native static function random(): Number 

        /**
            Round this number down to the closes integral value.
            @param num Number to round
            @return A rounded number
         */
        native static function round(num: Number): Number

        /**
            Calculates the sine of an angle (in radians).
            @param angle In radians 
            @return The sine of the argument 
         */
        native static function sin(angle: Number): Number 
        
        /**
            Calculates the square root of a number.
            @param oper The operand.
            @return The square root of the argument
         */
        native static function sqrt(oper: Number): Number 

        /**
            Calculates the tangent of an angle (in radians).
            @param angle In radians 
            @return The tangent of the argument 
         */
        native static function tan(angle: Number): Number 
    }
}

/*
    @copy   default
    
    Copyright (c) Embedthis Software LLC, 2003-2010. All Rights Reserved.
    Copyright (c) Michael O'Brien, 1993-2010. All Rights Reserved.
    
    This software is distributed under commercial and open source licenses.
    You may use the GPL open source license described below or you may acquire 
    a commercial license from Embedthis Software. You agree to be fully bound 
    by the terms of either license. Consult the LICENSE.TXT distributed with 
    this software for full details.
    
    This software is open source; you can redistribute it and/or modify it 
    under the terms of the GNU General Public License as published by the 
    Free Software Foundation; either version 2 of the License, or (at your 
    option) any later version. See the GNU General Public License for more 
    details at: http://www.embedthis.com/downloads/gplLicense.html
    
    This program is distributed WITHOUT ANY WARRANTY; without even the 
    implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
    
    This GPL license does NOT permit incorporating this software into 
    proprietary programs. If you are unable to comply with the GPL, you must
    acquire a commercial license to use this software. Commercial licenses 
    for this software and support services are available from Embedthis 
    Software at http://www.embedthis.com 
    
    @end
 */
/************************************************************************/
/*
 *  End of file "../src/es/core/Math.es"
 */
/************************************************************************/



/************************************************************************/
/*
 *  Start of file "../src/es/core/Name.es"
 */
/************************************************************************/

/*
 *  Name.es -- Name class
 *
 *  Copyright (c) All Rights Reserved. See details at the end of the file.
 */

module ejs {

    use default namespace intrinsic

    /**
        @hide
        @stability prototype
     */
    # ECMA
    native final class Name {
        use default namespace public

        const qualifier: Namespace
        const identifier: Namespace

        native function Name(qual: String, id: String = undefined)
    }
}

/*
 *  @copy   default
 *  
 *  Copyright (c) Embedthis Software LLC, 2003-2010. All Rights Reserved.
 *  Copyright (c) Michael O'Brien, 1993-2010. All Rights Reserved.
 *  
 *  This software is distributed under commercial and open source licenses.
 *  You may use the GPL open source license described below or you may acquire 
 *  a commercial license from Embedthis Software. You agree to be fully bound 
 *  by the terms of either license. Consult the LICENSE.TXT distributed with 
 *  this software for full details.
 *  
 *  This software is open source; you can redistribute it and/or modify it 
 *  under the terms of the GNU General Public License as published by the 
 *  Free Software Foundation; either version 2 of the License, or (at your 
 *  option) any later version. See the GNU General Public License for more 
 *  details at: http://www.embedthis.com/downloads/gplLicense.html
 *  
 *  This program is distributed WITHOUT ANY WARRANTY; without even the 
 *  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
 *  
 *  This GPL license does NOT permit incorporating this software into 
 *  proprietary programs. If you are unable to comply with the GPL, you must
 *  acquire a commercial license to use this software. Commercial licenses 
 *  for this software and support services are available from Embedthis 
 *  Software at http://www.embedthis.com 
 *  
 *  @end
 */
/************************************************************************/
/*
 *  End of file "../src/es/core/Name.es"
 */
/************************************************************************/



/************************************************************************/
/*
 *  Start of file "../src/es/core/Namespace.es"
 */
/************************************************************************/

/*
 *  Namespace.es -- Namespace class
 *
 *  Copyright (c) All Rights Reserved. See details at the end of the file.
 *
 *  NOTE: this is only partially implemented.
 */

module ejs {

    use default namespace intrinsic

    /**
        Namespaces are used to qualify names into discrete spaces
        @hide
        @stability prototype
     */
    native final class Namespace {

        use default namespace public
    }
}

/*
 *  @copy   default
 *  
 *  Copyright (c) Embedthis Software LLC, 2003-2010. All Rights Reserved.
 *  Copyright (c) Michael O'Brien, 1993-2010. All Rights Reserved.
 *  
 *  This software is distributed under commercial and open source licenses.
 *  You may use the GPL open source license described below or you may acquire 
 *  a commercial license from Embedthis Software. You agree to be fully bound 
 *  by the terms of either license. Consult the LICENSE.TXT distributed with 
 *  this software for full details.
 *  
 *  This software is open source; you can redistribute it and/or modify it 
 *  under the terms of the GNU General Public License as published by the 
 *  Free Software Foundation; either version 2 of the License, or (at your 
 *  option) any later version. See the GNU General Public License for more 
 *  details at: http://www.embedthis.com/downloads/gplLicense.html
 *  
 *  This program is distributed WITHOUT ANY WARRANTY; without even the 
 *  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
 *  
 *  This GPL license does NOT permit incorporating this software into 
 *  proprietary programs. If you are unable to comply with the GPL, you must
 *  acquire a commercial license to use this software. Commercial licenses 
 *  for this software and support services are available from Embedthis 
 *  Software at http://www.embedthis.com 
 *  
 *  @end
 */
/************************************************************************/
/*
 *  End of file "../src/es/core/Namespace.es"
 */
/************************************************************************/



/************************************************************************/
/*
 *  Start of file "../src/es/core/Null.es"
 */
/************************************************************************/

/*
 *  Null.es -- Null class used for the null value.
 *
 *  Copyright (c) All Rights Reserved. See details at the end of the file.
 */

module ejs {

    use default namespace intrinsic

    /**
     *  Base type for the null value. There is only one instance of the Null type and that is the null value.
     *  @spec ejs
     *  @stability evolving
     */
    native final class Null {

        /**
            Implementation artifacts
            @hide
         */
        override iterator native function get(): Iterator

        /**
            Implementation artifacts
            @hide
         */
        override iterator native function getValues(): Iterator
    }
}

/*
 *  @copy   default
 *  
 *  Copyright (c) Embedthis Software LLC, 2003-2010. All Rights Reserved.
 *  Copyright (c) Michael O'Brien, 1993-2010. All Rights Reserved.
 *  
 *  This software is distributed under commercial and open source licenses.
 *  You may use the GPL open source license described below or you may acquire 
 *  a commercial license from Embedthis Software. You agree to be fully bound 
 *  by the terms of either license. Consult the LICENSE.TXT distributed with 
 *  this software for full details.
 *  
 *  This software is open source; you can redistribute it and/or modify it 
 *  under the terms of the GNU General Public License as published by the 
 *  Free Software Foundation; either version 2 of the License, or (at your 
 *  option) any later version. See the GNU General Public License for more 
 *  details at: http://www.embedthis.com/downloads/gplLicense.html
 *  
 *  This program is distributed WITHOUT ANY WARRANTY; without even the 
 *  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
 *  
 *  This GPL license does NOT permit incorporating this software into 
 *  proprietary programs. If you are unable to comply with the GPL, you must
 *  acquire a commercial license to use this software. Commercial licenses 
 *  for this software and support services are available from Embedthis 
 *  Software at http://www.embedthis.com 
 *  
 *  @end
 */
/************************************************************************/
/*
 *  End of file "../src/es/core/Null.es"
 */
/************************************************************************/



/************************************************************************/
/*
 *  Start of file "../src/es/core/Number.es"
 */
/************************************************************************/

/*
    Number.es - Number class

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

module ejs {

    use default namespace intrinsic

    /**
        The Number type is used by all numeric values in Ejscript. Depending on how Ejscript is configured, the underlying
        number representation may be based on either an int, long, int64 or double data type. If the underlying type is
        integral (not double) then some of these routines may not be relevant.
        @stability evolving
     */
    native final class Number {

        use default namespace public

        /**
            Number constructor.
            @param value Value to use in creating the Number object. If the value cannot be converted to a number, 
                the value will ba NaN (or 0 if using integer numerics).
         */
        native function Number(value: Object = null)

        /**
            Return the maximim value this number type can assume. Alias for MaxValue.
            An object of the appropriate number with its value set to the maximum value allowed.
         */
        static const MAX_VALUE: Number = MaxValue

        /**
            Return the minimum value this number type can assume. Alias for MinValue.
            An object of the appropriate number with its value set to the minimum value allowed.
         */
        static const MIN_VALUE: Number = MinValue

        /**
            Not a Number. This is the result of an arithmetic expression which has no value.
         */
        static const NaN : Number = NaN

        /**
            Return a unique value which is less than or equal then any value which the number can assume. 
            @return A number with its value set to -Infinity. If the numeric type is integral, then return zero.
         */
        static const NEGATIVE_INFINITY: Number = NegativeInfinity

        /**
            Return a unique value which is greater then any value which the number can assume. 
            @return A number with its value set to Infinity. If the numeric type is integral, then return MaxValue.
         */
        static const POSITIVE_INFINITY: Number = Infinity

        /**
            Return the maximim value this number type can assume.
            @return A number with its value set to the maximum value allowed.
            @spec ejs
         */
        native static const MaxValue: Number

        /**
            Return the minimum value this number type can assume.
            @return A number with its value set to the minimum value allowed.
            @spec ejs
         */
        native static const MinValue: Number

        /**
            The absolute value of a number (which is equal to its magnitude).
            @spec ejs
         */
        function get abs(): Number
            Math.abs(this)

        /**
            The smallest integral number that is greater or equal to the number value. 
            @spec ejs
         */
        function get ceil(): Number 
            Math.ceil(this)

        /**
            The largest integral number that is smaller than the number value.
            @spec ejs
         */
        function get floor(): Number
            Math.floor(this)

        /**
            Is the number Infinity or -Infinity. Set to true or false.
            @spec ejs
         */
        native function get isFinite(): Boolean

        /**
            Is the number is equal to the NaN value. If the numeric type is integral, this will always return false.
            @spec ejs
         */
        native function get isNaN(): Boolean

        /**
            Compute the integral number that is closest to this number. ie. round up or down to the closest integer.
            @spec ejs
         */
        function get round(): Number
            Math.round(this)

        /**
            Returns the number formatted as a string in scientific notation with one digit before the decimal point 
            and the argument number of digits after it.
            @param fractionDigits The number of digits in the fraction.
            @return A string representing the number.
         */
        native function toExponential(fractionDigits: Number = 0): String

        /**
            Returns the number formatted as a string with the specified number of digits after the decimal point.
            @param fractionDigits The number of digits in the fraction.
            @return A string representing the number 
         */
        native function toFixed(fractionDigits: Number = 0): String

        /**
            Returns the number formatted as a string in either fixed or exponential notation with argument number of digits.
            @param numDigits The number of digits in the result. If omitted, the entire number is returned.
            @return A string
         */
        native function toPrecision(numDigits: Number = MAX_VALUE): String

        /**
            Byte sized integral number. Numbers are rounded and truncated as necessary.
            @spec ejs
         */
        function get byte(): Number
            integral(8)

        /**
            Convert this number to an integral value of the specified number of bits. Floating point numbers are 
                converted to integral values using truncation.
            @size Size in bits of the value
            @return An integral number
            @spec ejs
         */
        native function integral(size: Number = 32): Number

        /**
            Return an iterator that can be used to iterate a given number of times. This is used in for/in statements.
            @return an iterator
            @example
                for (i in 5) 
                    print(i)
            @spec ejs
         */
        override iterator native function get(): Iterator

        /**
            Return an iterator that can be used to iterate a given number of times. This is used in for/each statements.
            @return an iterator
            @example
                for each (i in 5) 
                    print(i)
            @spec ejs
         */
        override iterator native function getValues(): Iterator

        /**
            Returns the greater of the number and the arguments.
            @param other Other numbers number to compare with
            @return A number
            @spec ejs
         */
        function max(...other): Number {
            let result = this
            for each (n in other) {
                n = n cast Number
                if (n > result) {
                    result = n
                }
            }
            return result
        }

        /**
            Returns the lessor of the number and the arguments.
            @param other Numbers to compare with
            @return A number
            @spec ejs
         */
        function min(...other): Number {
            let result = this
            for each (n in other) {
                n = n cast Number
                if (n < result) {
                    result = n
                }
            }
            return result
        }

        /**
            Returns a number which is equal to this number raised to the power of the argument.
            @param nth Nth power to be raised to
            @return A number
            @spec ejs
         */
        function power(nth: Number): Number
            Math.pow(this, nth)

        /**
            This function converts the number to a string representation.
            @param radix Radix to use for the conversion. Defaults to 10. Non-default radixes are currently not supported.
            @returns a string representation of the number.
         */ 
        override native function toString(radix: Number = 10): String
    }
}

/*
    @copy   default
    
    Copyright (c) Embedthis Software LLC, 2003-2010. All Rights Reserved.
    Copyright (c) Michael O'Brien, 1993-2010. All Rights Reserved.
    
    This software is distributed under commercial and open source licenses.
    You may use the GPL open source license described below or you may acquire 
    a commercial license from Embedthis Software. You agree to be fully bound 
    by the terms of either license. Consult the LICENSE.TXT distributed with 
    this software for full details.
    
    This software is open source; you can redistribute it and/or modify it 
    under the terms of the GNU General Public License as published by the 
    Free Software Foundation; either version 2 of the License, or (at your 
    option) any later version. See the GNU General Public License for more 
    details at: http://www.embedthis.com/downloads/gplLicense.html
    
    This program is distributed WITHOUT ANY WARRANTY; without even the 
    implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
    
    This GPL license does NOT permit incorporating this software into 
    proprietary programs. If you are unable to comply with the GPL, you must
    acquire a commercial license to use this software. Commercial licenses 
    for this software and support services are available from Embedthis 
    Software at http://www.embedthis.com 
    
    @end
 */
/************************************************************************/
/*
 *  End of file "../src/es/core/Number.es"
 */
/************************************************************************/



/************************************************************************/
/*
 *  Start of file "../src/es/core/Object.es"
 */
/************************************************************************/

/*
 *  Object.es -- Object class. Base class for all types.
 *
 *  Copyright (c) All Rights Reserved. See details at the end of this file.
 */
module ejs {

    use default namespace intrinsic

    /**
     *  The Object Class is the root class from which all objects are based. It provides a foundation set of functions 
     *  and properties which are available to all objects. It provides for: copying objects, evaluating equality of 
     *  objects, providing information about base classes, serialization and deserialization and iteration. 
        @stability evolving
     */
    dynamic native class Object implements Iterable {

        use default namespace public

        /**
         *  Clone the object and all its elements.
         *  @param deep If true, do a deep copy where all object references are also copied, and so on, recursively.
         *  @spec ejs
         */
        native function clone(deep: Boolean = true): Object

        /**
         *  Get an iterator for this object to be used by "for (v in obj)"
         *  @return An iterator object.
         *  @spec ejs
         */
        iterator native function get(): Iterator

        /**
         *  Get an iterator for this object to be used by "for each (v in obj)"
         *  @return An iterator object.
         *  @spec ejs
         */
        iterator native function getValues(): Iterator

        /**
         *  The length of the object. For Objects, length() will be set to the number of properties. For Arrays, it will
         *  be set to the the number of elements. Other types will set length to the most natural representation of the
         *  size or length of the object.
         */
        native function get length(): Number 

        /**
         *  Convert an object to an equivalent JSON encoding.
         *  @return This function returns an object literal string.
         *  @throws TypeError If the object could not be converted to a string.
         */ 
        native function toJSON(): String

        /**
         *  This function converts an object to a string representation. Types typically override this to provide 
         *  the best string representation.
         *  @returns a string representation of the object. For Objects "[object className]" will be returned, 
         *  where className is set to the name of the class on which the object was based.
         */ 
        native function toString(): String
    }
}


/*
 *  @copy   default
 *  
 *  Copyright (c) Embedthis Software LLC, 2003-2010. All Rights Reserved.
 *  Copyright (c) Michael O'Brien, 1993-2010. All Rights Reserved.
 *  
 *  This software is distributed under commercial and open source licenses.
 *  You may use the GPL open source license described below or you may acquire 
 *  a commercial license from Embedthis Software. You agree to be fully bound 
 *  by the terms of either license. Consult the LICENSE.TXT distributed with 
 *  this software for full details.
 *  
 *  This software is open source; you can redistribute it and/or modify it 
 *  under the terms of the GNU General Public License as published by the 
 *  Free Software Foundation; either version 2 of the License, or (at your 
 *  option) any later version. See the GNU General Public License for more 
 *  details at: http://www.embedthis.com/downloads/gplLicense.html
 *  
 *  This program is distributed WITHOUT ANY WARRANTY; without even the 
 *  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
 *  
 *  This GPL license does NOT permit incorporating this software into 
 *  proprietary programs. If you are unable to comply with the GPL, you must
 *  acquire a commercial license to use this software. Commercial licenses 
 *  for this software and support services are available from Embedthis 
 *  Software at http://www.embedthis.com 
 *  
 *  @end
 */
/************************************************************************/
/*
 *  End of file "../src/es/core/Object.es"
 */
/************************************************************************/



/************************************************************************/
/*
 *  Start of file "../src/es/core/Reflect.es"
 */
/************************************************************************/

/*
 *  Reflect.es - Reflection API and class
 *
 *  Copyright (c) All Rights Reserved. See details at the end of the file.
 */

module ejs {

    use default namespace intrinsic

    /**
     *  Simple reflection class. This class is prototype and likely to change.
     *  @spec ejs
     *  @stability evolving 
     *  @example
     *      name      = Reflect(obj).name
     *      type      = Reflect(obj).type
     *      typeName  = Reflect(obj).typeName
     */
    native final class Reflect {
        use default namespace public

        native private var obj: Object

        /**
         *  Create a new reflection object.
         *  @param obj to reflect upon
         */
        native function Reflect(obj: Object)

        /**
         *  The name of the object
         */
        native function get name(): String

        /**
         *  The type of the object
         */
        native function get type(): Type

        /**
         *  The name of the type of the object
         */
        native function get typeName(): String
    }

    /**
     *  Return the name of a type. This is a fixed version of the standard "typeof" operator. It returns the real
     *  Ejscript underlying type. 
     *  This is implemented as a wrapper around Reflect(o).typeName.
     *  @param o Object or value to examine. 
     *  @return A string type name. If the object to examine is a type object, then return the name of the base type.
     *      If the object is Object, then return null.
     *  @spec ejs
     */
    function typeOf(o): String
        Reflect(o).typeName
}

/*
 *  @copy   default
 *  
 *  Copyright (c) Embedthis Software LLC, 2003-2010. All Rights Reserved.
 *  Copyright (c) Michael O'Brien, 1993-2010. All Rights Reserved.
 *  
 *  This software is distributed under commercial and open source licenses.
 *  You may use the GPL open source license described below or you may acquire 
 *  a commercial license from Embedthis Software. You agree to be fully bound 
 *  by the terms of either license. Consult the LICENSE.TXT distributed with 
 *  this software for full details.
 *  
 *  This software is open source; you can redistribute it and/or modify it 
 *  under the terms of the GNU General Public License as published by the 
 *  Free Software Foundation; either version 2 of the License, or (at your 
 *  option) any later version. See the GNU General Public License for more 
 *  details at: http://www.embedthis.com/downloads/gplLicense.html
 *  
 *  This program is distributed WITHOUT ANY WARRANTY; without even the 
 *  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
 *  
 *  This GPL license does NOT permit incorporating this software into 
 *  proprietary programs. If you are unable to comply with the GPL, you must
 *  acquire a commercial license to use this software. Commercial licenses 
 *  for this software and support services are available from Embedthis 
 *  Software at http://www.embedthis.com 
 *  
 *  @end
 */
/************************************************************************/
/*
 *  End of file "../src/es/core/Reflect.es"
 */
/************************************************************************/



/************************************************************************/
/*
 *  Start of file "../src/es/core/RegExp.es"
 */
/************************************************************************/

/*
 *  Regex.es -- Regular expression class.
 *
 *  Copyright (c) All Rights Reserved. See details at the end of the file.
 */

module ejs {

    use default namespace intrinsic

    /**
     *  Regular expressions per ECMA-262. The following special characters are supported:
     *  @spec evolving
     *  <table class="itemTable">
     *      <tr><td>\\</td><td>Reverse whether a character is treated literally or not.</td></tr>
     *      <tr><td>^</td><td>Match to the start of input. If matching multiline, match starting after a line break.</td></tr>
     *      <tr><td>\$ </td><td>Match to the end of input. If matching multiline, match before after a line break.</td></tr>
     *      <tr><td>*</td><td>Match the preceding item zero or more times.</td></tr>
     *      <tr><td>+</td><td>Match the preceding item one or more times.</td></tr>
     *      <tr><td>?</td><td>Match the preceding item zero or one times.</td></tr>
     *      <tr><td>(mem)</td><td>Match inside the parenthesis (i.e. "mem") and store the match.</td></tr>
     *      <tr><td>(?:nomem)</td><td>Match "nomem" and do not store the match.</td></tr>
     *      <tr><td>oper(?=need)</td><td>Match "oper" only if it is  followed by "need".</td></tr>
     *      <tr><td>oper(?!not)</td><td>Match "oper" only if it is not followed by "not".</td></tr>
     *      <tr><td>either|or</td><td>Match "either" or "or".</td></tr>
     *      <tr><td>{int}</td><td>Match exactly int occurences of the preceding item.</td></tr>
     *      <tr><td>{int,}</td><td>Match at least int occurences of the preceding item.</td></tr>
     *      <tr><td>{int1,int2}</td><td>Match at least int1 occurences of the preceding item but no more then int2.</td></tr>
     *      <tr><td>[pqr]</td><td>Match any one of the enclosed characters. Use a hyphen to specify a range of characters.</td></tr>
     *      <tr><td>[^pqr]</td><td>Match anything except the characters in brackets.</td></tr>
     *      <tr><td>[\b]</td><td>Match a backspace.</td></tr>
     *      <tr><td>\b</td><td>Match a word boundary.</td></tr>
     *      <tr><td>\B</td><td>Match a non-word boundary.</td></tr>
     *      <tr><td>\cQ</td><td>Match a control string, e.g. Control-Q</td></tr>
     *      <tr><td>\d</td><td>Match a digit.</td></tr>
     *      <tr><td>\D</td><td>Match any non-digit character.</td></tr>
     *      <tr><td>\f</td><td>Match a form feed.</td></tr>
     *      <tr><td>\n</td><td>Match a line feed.</td></tr>
     *      <tr><td>\r</td><td>Match a carriage return.</td></tr>
     *      <tr><td>\s</td><td>Match a single white space.</td></tr>
     *      <tr><td>\S</td><td>Match a non-white space.</td></tr>
     *      <tr><td>\t</td><td>Match a tab.</td></tr>
     *      <tr><td>\v</td><td>Match a vertical tab.</td></tr>
     *      <tr><td>\w</td><td>Match any alphanumeric character.</td></tr>
     *      <tr><td>\W</td><td>Match any non-word character.</td></tr>
     *      <tr><td>\int</td><td>A reference back int matches.</td></tr>
     *      <tr><td>\0</td><td>Match a null character.</td></tr>
     *      <tr><td>\xYY</td><td>Match the character code YY.</td></tr>
     *      <tr><td>\xYYYY</td><td>Match the character code YYYY.</td></tr>
     *  </table>
     */
    native final class RegExp {

        use default namespace public

        /**
         *  Create a regular expression object that can be used to process strings.
         *  @param pattern The pattern to associated with this regular expression.
         *  @param flags "g" for global match, "i" to ignore case, "m" match over multiple lines, "y" for sticky match.
         */
        native function RegExp(pattern: String, flags: String = null)

        /**
         *  The integer index of the end of the last match plus one. This is the index to start the next match for
         *  global patterns. This is only set if the "g" flag was used.
         *  It is set to the match ending index plus one. Set to zero if no match.
         */
        native function get lastIndex(): Number

        /**
         *  Set the integer index of the end of the last match plus one. This is the index to start the next match for
         *  global patterns.
         *  @return Match end plus one. Set to zero if no match.
         */
        native function set lastIndex(value: Number): Void

        /**
         *  Match this regular expression against the supplied string. By default, the matching starts at the beginning 
         *  of the string.
         *  @param str String to match.
         *  @param start Optional starting index for matching.
         *  @return Array of results, empty array if no matches.
         *  @spec ejs Adds start argument.
         */
        native function exec(str: String, start: Number = 0): Array

        /**
         *  Global flag. If the global modifier was specified, the regular expression will search through the entire 
         *  input string looking for matches.
         *  @spec ejs
         */
        native function get global(): Boolean

        /**
         *  Ignore case flag. If the ignore case modifier was specifed, the regular expression is case insensitive.
         *  @return The case flag, true if set, false otherwise.
         *  @spec ejs
         */
        native function get ignoreCase(): Boolean

        /**
         *  Multiline flag. If the multiline modifier was specified, the regular expression will search through 
         *  carriage return and new line characters in the input.
         */
        native function get multiline(): Boolean

        /**
         *  Regular expression source pattern currently set
         */
        native function get source(): String

        /**
         *  Substring last matched. Set to the matched string or null if there were no matches.
         *  @spec ejs
         */
        native function get matched(): String

        /**
         *  Replace all the matches. This call replaces all matching substrings with the corresponding array element.
         *  If the array element is not a string, it is converted to a string before replacement.
         *  @param str String to match and replace.
         *  @param replacement Replacement text
         *  @return A string with zero, one or more substitutions in it.
         *  @spec ejs
         */
        function replace(str: String, replacement: Object): String
            str.replace(this, replacement)

        /**
         *  Split the target string into substrings around the matching sections.
         *  @param target String to split.
         *  @return Array containing the matching substrings
         *  @spec ejs
         */
        function split(target: String): Array
            target.split(this)

        /**
         *  Integer index of the start of the last match. This is only set if the "g" flag was used.
         *  @spec ejs
         */
        native function get start(): Number

        /**
         *  Sticky flag. If the sticky modifier was specified, the regular expression will only match from the $lastIndex.
         *  @spec ejs
         */
        native function get sticky(): Boolean

        /**
         *  Test whether this regular expression will match against a string.
         *  @param str String to search.
         *  @return True if there is a match, false otherwise.
         *  @spec ejs
         */
        native function test(str: String): Boolean

        /**
         *  Convert the regular expression to a string
         *  @returns a string representation of the regular expression.
         */
        override native function toString(): String
    }
}

/*
 *  @copy   default
 *  
 *  Copyright (c) Embedthis Software LLC, 2003-2010. All Rights Reserved.
 *  Copyright (c) Michael O'Brien, 1993-2010. All Rights Reserved.
 *  
 *  This software is distributed under commercial and open source licenses.
 *  You may use the GPL open source license described below or you may acquire 
 *  a commercial license from Embedthis Software. You agree to be fully bound 
 *  by the terms of either license. Consult the LICENSE.TXT distributed with 
 *  this software for full details.
 *  
 *  This software is open source; you can redistribute it and/or modify it 
 *  under the terms of the GNU General Public License as published by the 
 *  Free Software Foundation; either version 2 of the License, or (at your 
 *  option) any later version. See the GNU General Public License for more 
 *  details at: http://www.embedthis.com/downloads/gplLicense.html
 *  
 *  This program is distributed WITHOUT ANY WARRANTY; without even the 
 *  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
 *  
 *  This GPL license does NOT permit incorporating this software into 
 *  proprietary programs. If you are unable to comply with the GPL, you must
 *  acquire a commercial license to use this software. Commercial licenses 
 *  for this software and support services are available from Embedthis 
 *  Software at http://www.embedthis.com 
 *  
 *  @end
 */
/************************************************************************/
/*
 *  End of file "../src/es/core/RegExp.es"
 */
/************************************************************************/



/************************************************************************/
/*
 *  Start of file "../src/es/core/Stream.es"
 */
/************************************************************************/

/*
 *  Stream.es -- Stream class. Base interface implemented by Streams.
 *
 *  Copyright (c) All Rights Reserved. See details at the end of the file.
 */

module ejs {

    use default namespace intrinsic

    /**
     *  Stream objects represent bi-directional streams of data that pass data elements between an endpoint known 
     *  as a source or sink and a consumer / producer. In between, intermediate streams may be used as filters. 
     *  Example endpoints are the File, Socket, String and Http classes. The TextStream is an example of a filter 
     *  stream. The data elements passed by streams may be any series of objects including: bytes, lines of text, 
     *  integers or objects. Streams may buffer the incoming data or not. Streams may offer sync and/or async modes 
     *  of operation.
     *  @spec ejs
     *  @spec evolving
     */
    interface Stream {

        use default namespace public

        /**
         *  Close the input stream and free up all associated resources.
         *  WARNING: This API will have the graceful parameter removed in a future release. Call $flush manually if
         *  you require graceful closes.
         *  @param graceful if true, then close the socket gracefully after writing all pending data.
         *  @prototype
         */
        function close(graceful: Boolean = false): Void

        /**
         *  Flush the stream and all stacked streams and underlying data source/sinks.
         *  WARNING: This API will have the graceful parameter removed in a future release. All flushes will then
         *  be graceful.
         *  @param graceful if true, then write all pending data.
         *  @prototype
         */
        function flush(graceful: Boolean = true): Void 

        /**
         *  Read a block of data from the stream. Read the required number of bytes from the stream into the 
         *      supplied byte array at the given offset. 
         *  @param buffer Destination byte array for read data.
         *  @param offset Offset in the byte array to place the data. If the offset is -1, then data is
         *      appended to the buffer write $position which is then updated. 
         *  @param count Number of bytes to read. If -1, read as much as the buffer will hold up to the entire 
         *      stream if the buffer is of sufficient size or is growable.
         *  @returns a count of the bytes actually read.
         *  @throws IOError if an I/O error occurs.
         */
        function read(buffer: ByteArray, offset: Number = 0, count: Number = -1): Number 

        /**
         *  Write data to the stream. If in sync mode, the write call blocks until the underlying stream or 
         *  endpoint absorbes all the data. If in async-mode, the call accepts whatever data can be accepted 
         *  immediately and returns a count of the elements that have been written.
         *  @param data Data to write. 
         *  @returns The total number of elements that were written.
         *  @throws IOError if there is an I/O error.
         */
        function write(... data): Number
    }
}


/*
 *  @copy   default
 *  
 *  Copyright (c) Embedthis Software LLC, 2003-2010. All Rights Reserved.
 *  Copyright (c) Michael O'Brien, 1993-2010. All Rights Reserved.
 *  
 *  This software is distributed under commercial and open source licenses.
 *  You may use the GPL open source license described below or you may acquire 
 *  a commercial license from Embedthis Software. You agree to be fully bound 
 *  by the terms of either license. Consult the LICENSE.TXT distributed with 
 *  this software for full details.
 *  
 *  This software is open source; you can redistribute it and/or modify it 
 *  under the terms of the GNU General Public License as published by the 
 *  Free Software Foundation; either version 2 of the License, or (at your 
 *  option) any later version. See the GNU General Public License for more 
 *  details at: http://www.embedthis.com/downloads/gplLicense.html
 *  
 *  This program is distributed WITHOUT ANY WARRANTY; without even the 
 *  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
 *  
 *  This GPL license does NOT permit incorporating this software into 
 *  proprietary programs. If you are unable to comply with the GPL, you must
 *  acquire a commercial license to use this software. Commercial licenses 
 *  for this software and support services are available from Embedthis 
 *  Software at http://www.embedthis.com 
 *  
 *  @end
 */
/************************************************************************/
/*
 *  End of file "../src/es/core/Stream.es"
 */
/************************************************************************/



/************************************************************************/
/*
 *  Start of file "../src/es/core/String.es"
 */
/************************************************************************/

/*
    String.es -- String class
 *
    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

module ejs {

    use default namespace intrinsic

    /**
        Each String object represents a single immutable linear sequence of characters. Strings have operators 
        for: comparison, concatenation, copying, searching, conversion, matching, replacement, and, subsetting.
        @stability evolving
     */
    native final class String {

        use default namespace public

        /**
            String constructor. This can take two forms:
            <ul>
                <li>String()</li>
                <li>String(str: String)</li>
            </ul>
            @param str The args can be either empty or a string. If a non-string arg is supplied, the VM will automatically
                cast to a string.
         */
        native function String(...str)

        /**
            Do a case sensitive comparison between this string and another.
            @param compare The string to compare against
            @return -1 if this string is less than the compare string, zero if equal and 1 if greater than.
            @spec ejs
         */
        native function caseCompare(compare: String): Number

        /**
            Return the character at a given position in the string
            @returns a new string containing the selected character. If the index is out of range, returns the empty string.
         */
        native function charAt(index: Number): String

        /**
            Get a character code. 
            @param index The index of the character to retrieve
            @return Return the character code at the specified index. If the index is -1, get the last character.
                Return NaN if the index is out of range.
         */
        native function charCodeAt(index: Number = 0): Number

        /**
            Concatenate strings and returns a new string. 
            @param args Strings to append to this string
            @return Return a new string.
         */
        native function concat(...args): String

        /**
            Check if a string contains a pattern.
            @param pattern The pattern can be either a string or regular expression.
            @return Returns true if the pattern is found.
            @spec ejs
         */
        native function contains(pattern: Object): Boolean

        /**
            Determine if this string ends with a given string
            @param test The string to test with
            @return True if the string matches.
            @spec ejs
         */
        native function endsWith(test: String): Boolean

        /**
            Format arguments as a string. Use the string as a format specifier.
            @param args Array containing the data to format. 
            @return -1 if less than, zero if equal and 1 if greater than.
            @example
                "%5.3f".format(num)
            \n\n
                "%s Arg1 %d, arg2 %d".format("Hello World", 1, 2)
            @spec ejs
         */
        native function format(...args): String

        /**
            Create a string from the character code arguments
            @param codes Character codes from which to create the string
            @returns a new string
         */
        native static function fromCharCode(...codes): String

        /**
            Get an iterator for this array to be used by "for (v in string)"
            @return An iterator object.
         */
        override iterator native function get(): Iterator

        /**
            Get an iterator for this array to be used by "for each (v in string)"
            @return An iterator object.
         */
        override iterator native function getValues(): Iterator

        /**
            Search for an item using strict equality "===". This call searches from the start of the string for 
            the specified element. 
            @param pattern The string to search for.
            @param startIndex Where in the array (zero based) to start searching for the object.
            @return The items index into the array if found, otherwise -1.
         */
        native function indexOf(pattern: String, startIndex: Number = 0): Number

        /**
            Is there is at least one character in the string and all characters are digits.
            @spec ejs
         */
        native function get isDigit(): Boolean

        /**
            Is there is at least one character in the string and all characters are alphabetic.
            @spec ejs
         */
        native function get isAlpha(): Boolean

        /**
            Is there is at least one character in the string and there are no upper case characters.
            @spec ejs
         */
        native function get isLower(): Boolean

        /**
            Is there is at least one character in the string and all characters are white space.
            @spec ejs
         */
        native function get isSpace(): Boolean

        /**
            If there is at least one character in the string and there are no lower case characters.
            @spec ejs
         */
        native function get isUpper(): Boolean

        /**
            Search right to left for a substring starting at a given index.
            @param pattern The string to search for
            @param location The integer starting to start the search or a range to search in.
            @return Return the starting index of the last match found.
         */
        native function lastIndexOf(pattern: String, location: Number = -1): Number

        /**
            The length of the string in bytes.
         */
        override native function get length(): Number

        /**
            Match the a regular expression pattern against a string.
            @param pattern The regular expression to search for
            @return Returns an array of matching substrings.
         */
        native function match(pattern: RegExp): Array

        /**
            Parse the current string object as a JSON string object. The @filter is an optional filter with the 
            following signature:
                function filter(key: String, value: String): Boolean
            @param filter Optional function to call for each element of objects and arrays. Not yet supported.
            @returns an object representing the JSON string.
         */
        function parseJSON(filter: Function = null): Object
            deserialize(this)

        /**
            Copy the string into a new string and lower case the first character if there is one. If the first non-white 
            character is not a character or if it is already lower there is no change.
            @return A new String
            @spec ejs
         */
        native function toCamel(): String

        /**
            Copy the string into a new string and capitalize the first character if there is one. If the first non-white 
            character is not a character or if it is already capitalized there is no change.
            @return A new String
            @spec ejs
         */
        native function toPascal(): String

        /**
            Create a new string with all nonprintable characters replaced with unicode hexadecimal equivalents (e.g. \uNNNN).
            @return The new string
            @spec ejs
         */
        native function printable(): String

        /**
            Wrap a string in double quotes.
            @return The new string
         */
        native function quote(): String

        /**
            Remove characters from a string. Remove the elements from @start to @end inclusive. 
            @param start Numeric index of the first element to remove. Negative indicies measure from the end of the string.
            -1 is the last character element.
            @param end Numeric index of one past the last element to remove
            @return A new string with the characters removed
            @spec ejs
         */
        native function remove(start: Number, end: Number = -1): String

        /**
            Search and replace. Search for the given pattern which can be either a string or a regular expression 
            and replace it with the replace text. If the pattern is a string, only the first occurrence is replaced.
            @param pattern The regular expression pattern to search for
            @param replacement The string to replace the match with or a function to generate the replacement text
            @return Returns a new string.
            @spec ejs
         */
        native function replace(pattern: Object, replacement: String): String

        /**
            Reverse a string. 
            @return Returns a new string with the order of all characters reversed.
            @spec ejs
         */
        native function reverse(): String

        /**
            Search for a pattern.
            @param pattern Regular expression pattern to search for in the string.
            @return Return the starting index of the pattern in the string. Returns -1 if not found.
         */
        native function search(pattern: Object): Number

        /**
            Extract a substring.
            @param start The position of the first character to slice.
            @param end The position one after the last character. Negative indicies are measured from the end of the string.
            @param step Extract every "step" character.
            @throws OutOfBoundsError If the range boundaries exceed the string limits.
         */ 
        native function slice(start: Number, end: Number = -1, step: Number = 1): String

        /**
            Split a string into an array of substrings. Tokenizes a string using a specified delimiter.
            @param delimiter String or regular expression object separating the tokens.
            @param limit At most limit strings are extracted. If limit is set to -1, then unlimited strings are extracted.
            @return Returns an array of strings.
         */
        native function split(delimiter: Object, limit: Number = -1): Array

        /**
            Tests if this string starts with the string specified in the argument.
            @param test String to compare against
            @return True if it does, false if it doesn't
            @spec ejs
         */
        native function startsWith(test: String): Boolean

        /**
            Extract a substring. Similar to slice but only allows positive indicies.
            @param startIndex Integer location to start copying
            @param end Postitive index of one past the last character to extract.
            @return Returns a new string
            @throws OutOfBoundsError If the starting index and/or the length exceed the string's limits.
         */
        native function substring(startIndex: Number, end: Number = -1): String

        /**
            Replication. Replicate the string N times.
            @param times The number of times to copy the string
            @return A new String with the copies concatenated together. Returns the empty string if times is <= zero.
            @spec ejs
         */
        function times(times: Number): String {
            var s: String = ""

            for (i in times) {
                s += this
            }
            return s
        }

        /**
            Scan the input and tokenize according to a string format specifier.
            @param format Tokenizing format specifier
            @returns array containing the tokenized elements
            @example
                for (s in string.tokenize("%s %s %s")) {
                    print(s)
                }
            @spec ejs
         */
        native function tokenize(format: String): Array

        /**
            Convert the string to an equivalent JSON encoding.
            @return A quoted string.
            @throws TypeError If the object could not be converted to a string.
         */ 
        override native function toJSON(): String

        /**
            Convert the string to lower case.
            @return Returns a new lower case version of the string.
            @spec ejs
         */
        native function toLower(locale: String = null): String

        /**
            This function converts an object to a string representation. Types typically override this to provide 
            the best string representation.
            @returns a string representation of the object. For Objects "[object className]" will be returned, 
            where className is set to the name of the class on which the object was based.
         */ 
        override native function toString(locale: String = null): String

        /**
            Convert the string to upper case.
            @return Returns a new upper case version of the string.
            @spec ejs
         */
        native function toUpper(locale: String = null): String

        /**
            Returns a trimmed copy of the string. Normally used to trim white space, but can be used to trim any 
            substring from the start or end of the string.
            @param str May be set to a substring to trim from the string. If not set, it defaults to any white space.
            @return Returns a (possibly) modified copy of the string
         */
        native function trim(str: String = null): String

        /*
            Overloaded operators
         */

        /**
            String subtraction. Remove the first occurance of str.
            @param str The string to remove from this string
            @return Return a new string.
            @spec ejs
         */
        function - (str: String): String {
            var i: Number = indexOf(str)
            return remove(i, i + str.length)
        }
        
        /**
            Compare strings. 
            @param str The string to compare to this string
            @return -1 if less than, zero if equal and 1 if greater than.
            @spec ejs
         */
        # DOC_ONLY
        function < (str: String): Number {
            return localeCompare(str) < 0
        }

        /**
            Compare strings.
            @param str The string to compare to this string
            @return -1 if less than, zero if equal and 1 if greater than.
            @spec ejs
         */
        # DOC_ONLY
        function > (str: String): Number {
            return localeCompare(str) > 0
        }

        /**
            Compare strings.
            @param str The string to compare to this string
            @return -1 if less than, zero if equal and 1 if greater than.
            @spec ejs
         */
        # DOC_ONLY
        function == (str: String): Number {
            return localeCompare(str) == 0
        }

        /**
            Format arguments as a string. Use the string as a format specifier.
            @param arg The argument to format. Pass an array if multiple arguments are required.
            @return -1 if less than, zero if equal and 1 if greater than.
            @example
                "%5.3f" % num
            <br/>
                "Arg1 %d, arg2 %d" % [1, 2]
            @spec ejs
         */
        function % (arg: Object): String {
            return format(arg)
        }
    }
}


/*
    @copy   default
    
    Copyright (c) Embedthis Software LLC, 2003-2010. All Rights Reserved.
    Copyright (c) Michael O'Brien, 1993-2010. All Rights Reserved.
    
    This software is distributed under commercial and open source licenses.
    You may use the GPL open source license described below or you may acquire 
    a commercial license from Embedthis Software. You agree to be fully bound 
    by the terms of either license. Consult the LICENSE.TXT distributed with 
    this software for full details.
    
    This software is open source; you can redistribute it and/or modify it 
    under the terms of the GNU General Public License as published by the 
    Free Software Foundation; either version 2 of the License, or (at your 
    option) any later version. See the GNU General Public License for more 
    details at: http://www.embedthis.com/downloads/gplLicense.html
    
    This program is distributed WITHOUT ANY WARRANTY; without even the 
    implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
    
    This GPL license does NOT permit incorporating this software into 
    proprietary programs. If you are unable to comply with the GPL, you must
    acquire a commercial license to use this software. Commercial licenses 
    for this software and support services are available from Embedthis 
    Software at http://www.embedthis.com 
    
    @end
 */
/************************************************************************/
/*
 *  End of file "../src/es/core/String.es"
 */
/************************************************************************/



/************************************************************************/
/*
 *  Start of file "../src/es/core/Type.es"
 */
/************************************************************************/

/*
 *  Type.es -- Type class. Base class for all type objects.
 *
 *  Copyright (c) All Rights Reserved. See details at the end of the file.
 */

module ejs {

    use default namespace intrinsic

    /*
        WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING
        WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING
        WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING
        WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING
        Must not define properties and methods of this type. They cannot be inherited by types.
     */

    /**
     *  Base class for all type objects.
     *  @spec ejs
        @spec prototype
     */
    native final class Type {
    }
}

/*
 *  @copy   default
 *  
 *  Copyright (c) Embedthis Software LLC, 2003-2010. All Rights Reserved.
 *  Copyright (c) Michael O'Brien, 1993-2010. All Rights Reserved.
 *  
 *  This software is distributed under commercial and open source licenses.
 *  You may use the GPL open source license described below or you may acquire 
 *  a commercial license from Embedthis Software. You agree to be fully bound 
 *  by the terms of either license. Consult the LICENSE.TXT distributed with 
 *  this software for full details.
 *  
 *  This software is open source; you can redistribute it and/or modify it 
 *  under the terms of the GNU General Public License as published by the 
 *  Free Software Foundation; either version 2 of the License, or (at your 
 *  option) any later version. See the GNU General Public License for more 
 *  details at: http://www.embedthis.com/downloads/gplLicense.html
 *  
 *  This program is distributed WITHOUT ANY WARRANTY; without even the 
 *  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
 *  
 *  This GPL license does NOT permit incorporating this software into 
 *  proprietary programs. If you are unable to comply with the GPL, you must
 *  acquire a commercial license to use this software. Commercial licenses 
 *  for this software and support services are available from Embedthis 
 *  Software at http://www.embedthis.com 
 *  
 *  @end
 */
/************************************************************************/
/*
 *  End of file "../src/es/core/Type.es"
 */
/************************************************************************/



/************************************************************************/
/*
 *  Start of file "../src/es/core/Void.es"
 */
/************************************************************************/

/*
 *  Void.es -- Void class used for undefined value.
 *
 *  Copyright (c) All Rights Reserved. See details at the end of the file.
 */

module ejs {

    use default namespace intrinsic

    /**
     *  The Void type is the base class for the undefined value. One instance of this type is created for the 
     *  system's undefined value.
     *  @spec ejs
     *  @spec evolving
     */
    native final class Void {
        /**
         *  Implementation artifacts
         *  @hide
         */
        override iterator native function get(): Iterator

        /**
         *  Implementation artifacts
         *  @hide
         */
        override iterator native function getValues(): Iterator
    }
}


/*
 *  @copy   default
 *  
 *  Copyright (c) Embedthis Software LLC, 2003-2010. All Rights Reserved.
 *  Copyright (c) Michael O'Brien, 1993-2010. All Rights Reserved.
 *  
 *  This software is distributed under commercial and open source licenses.
 *  You may use the GPL open source license described below or you may acquire 
 *  a commercial license from Embedthis Software. You agree to be fully bound 
 *  by the terms of either license. Consult the LICENSE.TXT distributed with 
 *  this software for full details.
 *  
 *  This software is open source; you can redistribute it and/or modify it 
 *  under the terms of the GNU General Public License as published by the 
 *  Free Software Foundation; either version 2 of the License, or (at your 
 *  option) any later version. See the GNU General Public License for more 
 *  details at: http://www.embedthis.com/downloads/gplLicense.html
 *  
 *  This program is distributed WITHOUT ANY WARRANTY; without even the 
 *  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
 *  
 *  This GPL license does NOT permit incorporating this software into 
 *  proprietary programs. If you are unable to comply with the GPL, you must
 *  acquire a commercial license to use this software. Commercial licenses 
 *  for this software and support services are available from Embedthis 
 *  Software at http://www.embedthis.com 
 *  
 *  @end
 */
/************************************************************************/
/*
 *  End of file "../src/es/core/Void.es"
 */
/************************************************************************/



/************************************************************************/
/*
 *  Start of file "../src/es/db/Database.es"
 */
/************************************************************************/

/**
 *  Database.es -- Database class
 *
 *  Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/*
 *  Design notes:
 *  - Don't create static vars so this class can be fully stateless and can be placed into the master interpreter.
 */

module ejs.db {

    /**
     *  SQL Database support. The Database class provides an interface over other database adapter classes such as 
     *  SQLite or MySQL. Not all the functionality expressed by this API may be implemented by a specific database adapter.
     *  @spec ejs
     *  @stability evolving
     */
    class Database {

        private var _adapter: Object
        private var _connection: String
        private var _name: String
        private var _traceAll: Boolean

        use default namespace public

        /**
         *  Initialize a database connection using the supplied database connection string
            @param adapter Database adapter to use. E.g. "sqlite"
         *  @param connectionString Connection string stipulating how to connect to the database. The format is one of the 
         *  following forms:
         *      <ul>
         *          <li>adapter://host/database/username/password</li>
         *          <li>filename</li>
         *      </ul>
         *      Where adapter specifies the kind of database. Sqlite is currently the only supported adapter.
         *      For sqlite connection strings, the abbreviated form is permitted where a filename is supplied and the 
         *      connection string is assumed to be: <pre>sqlite://localhost/filename</pre>
         */
        function Database(adapter: String, connectionString: String) {
            if (adapter == "sqlite3") adapter = "sqlite"
            try {
                _name = basename(connectionString)
                _connection = connectionString
                let adapterClass = adapter.toPascal()
                if (global."ejs.db"::[adapterClass] == undefined) {
                    load("ejs.db." + adapter + ".mod")
                }
                _adapter = new global."ejs.db"::[adapterClass](connectionString)
            } catch (e) {
                print(e)
                throw "Can't find database connector for " + adapter
            }
        }

        /**
         *  Add a column to a table.
         *  @param table Name of the table
         *  @param column Name of the column to add
         *  @param datatype Database independant type of the column. Valid types are: binary, boolean, date,
         *      datetime, decimal, float, integer, number, string, text, time and timestamp.
         *  @param options Optional parameters
         */
        function addColumn(table: String, column: String, datatype: String, options: Object = null): Void
            _adapter.addColumn(table, column, datatype, options)

        /**
         *  Add an index on a column
         *  @param table Name of the table
         *  @param column Name of the column to add
         *  @param index Name of the index
         */
        function addIndex(table: String, column: String, index: String): Void
            _adapter.addIndex(table, column, index)

        /**
         *  Change a column
         *  @param table Name of the table holding the column
         *  @param column Name of the column to change
         *  @param datatype Database independant type of the column. Valid types are: binary, boolean, date,
         *      datetime, decimal, float, integer, number, string, text, time and timestamp.
         *  @param options Optional parameters
         */
        function changeColumn(table: String, column: String, datatype: String, options: Object = null): Void
            _adapter.changeColumn(table, column, datatype, options)

        /**
         *  Close the database connection. Database connections should be closed when no longer needed rather than waiting
         *  for the garbage collector to automatically close the connection when disposing the database instance.
         */
        function close(): Void
            _adapter.close()

        /**
         *  Commit a database transaction
         */
        function commit(): Void
            _adapter.commit()

        /**
         *  Reconnect to the database using a new connection string. Not yet implemented.
         *  @param connectionString See Database() for information about connection string formats.
         *  @hide
         */
        function connect(connectionString: String): Void
            _adapter.connect(connectionString)

        /**
         *  The database connection string
         */
        function get connection(): String {
            return _connection
        }

        /**
         *  Create a new database
         *  @param name Name of the database
         *  @options Optional parameters
         */
        function createDatabase(name: String, options: Object = null): Void
            _adapter.createDatabase(name, options)

        /**
         *  Create a new table
         *  @param table Name of the table
         *  @param columns Array of column descriptor tuples consisting of name:datatype
         *  @options Optional parameters
         */
        function createTable(table: String, columns: Array = null): Void
            _adapter.createTable(table, columns)

        /**
         *  Map the database independant data type to a database dependant SQL data type
         *  @param dataType Data type to map
         *  @returns The corresponding SQL database type
         */
        function dataTypeToSqlType(dataType:String): String
            _adapter.dataTypeToSqlType(dataType)

        /**
         *  The default database for the application.
         */
        static function get defaultDatabase(): Database
            global."ejs.db"::"defaultDb"

        /**
         *  Set the default database for the application.
         *  @param db the default database to define
         */
        static function set defaultDatabase(db: Database): Void {
            /*
             *  Do this rather than using a Database static var so Database can go into the master interpreter
             */
            global."ejs.db"::"defaultDb" = db
        }

        /**
         *  Destroy a database
         *  @param name Name of the database to remove
         */
        function destroyDatabase(name: String): Void
            _adapter.destroyDatabase(name)

        /**
         *  Destroy a table
         *  @param table Name of the table to destroy
         */
        function destroyTable(table: String): Void
            _adapter.destroyTable(table)

        /**
         *  End a transaction
         */
        function endTransaction(): Void
            _adapter.endTransaction()

        /**
         *  Get column information 
         *  @param table Name of the table to examine
         *  @return An array of column data. This is database specific content and will vary depending on the
         *      database connector in use.
         */
        function getColumns(table: String): Array
            _adapter.getColumns(table)

        /**
         *  Return list of tables in a database
         *  @returns an array containing list of table names present in the currently opened database.
         */
        function getTables(): Array
            _adapter.getTables()

        /**
         *  Return the number of rows in a table
         *  @returns the count of rows in a table in the currently opened database.
         */
        function getNumRows(table: String): Number
            _adapter.getNumRows()

        /**
         *  The database name defined via the connection string or constructor.
         */
        function get name(): String
            _name

        /**
         *  Execute a SQL command on the database.
         *  @param cmd SQL command string
            @param tag Debug tag to use when logging the command
            @param trace Set to true to eanble logging this command.
         *  @returns An array of row results where each row is represented by an Object hash containing the 
         *      column names and values
         */
        function query(cmd: String, tag: String = "SQL", trace: Boolean = false): Array {
            if (_traceAll || trace) {
                print(tag + ": " + cmd)
            }
            return _adapter.sql(cmd)
        }

        /**
         *  Remove columns from a table
         *  @param table Name of the table to modify
         *  @param columns Array of column names to remove
         */
        function removeColumns(table: String, columns: Array): Void
            _adapter.removeColumns(table, columns)

        /**
         *  Remove an index
         *  @param table Name of the table to modify
         *  @param index Name of the index to remove
         */
        function removeIndex(table: String, index: String): Void
            _adapter.removeIndex(table, index)

        /**
         *  Rename a column
         *  @param table Name of the table to modify
         *  @param oldColumn Old column name
         *  @param newColumn New column name
         */
        function renameColumn(table: String, oldColumn: String, newColumn: String): Void
            _adapter.renameColumn(table, oldColumn, newColumn)

        /**
         *  Rename a table
         *  @param oldTable Old table name
         *  @param newTable New table name
         */
        function renameTable(oldTable: String, newTable: String): Void
            _adapter.renameTable(oldTable, newTable)

        /**
         *  Rollback an uncommited database transaction. Not supported.
         *  @hide
         */
        function rollback(): Void
            _adapter.rollback()

        /**
         *  Execute a SQL command on the database. This is a low level SQL command interface that bypasses logging.
         *      Use @query instead.
         *  @param cmd SQL command string
         *  @returns An array of row results where each row is represented by an Object hash containing the column 
         *      names and values
         */
        function sql(cmd: String): Array
            _adapter.sql(cmd)

        /**
         *  Map the SQL type to a database independant data type
         *  @param sqlType SQL Data type to map
         *  @returns The corresponding database independant type
         */
        function sqlTypeToDataType(sqlType: String): String
            _adapter.sqlTypeToDataType(sqlType)

        /**
         *  Map the SQL type to an Ejscript type class
         *  @param sqlType SQL Data type to map
         *  @returns The corresponding type class
         */
        function sqlTypeToEjsType(sqlType: String): Type
            _adapter.sqlTypeToEjsType(sqlType)

        /**
         *  Start a new database transaction
         */
        function startTransaction(): Void
            _adapter.startTransaction()

        /**
         *  Trace all SQL statements on this database. Control whether trace is enabled for all SQL statements 
         *  issued against the database.
         *  @param on If true, display each SQL statement to the log
         */
        function trace(on: Boolean): void
            _traceAll = on

        /**
         *  Execute a database transaction
         *  @param code Function to run inside a database transaction
         */
        function transaction(code: Function): Void {
            startTransaction()
            try {
                code()
            } catch (e: Error) {
                rollback();
            } finally {
                endTransaction()
            }
        }

        /**
            Quote ", ', --, ;
            @hide
         */
        static function quote(str: String): String  {
            // str.replace(/'/g, "''").replace(/[#;\x00\x1a\r\n",;\\-]/g, "\\$0")
            // return str.replace(/'/g, "''").replace(/[#;",;\\-]/g, "\\$0")
            // return str.replace(/'/g, "''").replace(/[#";\\]/g, "\\$0")
            // return str.replace(/'/g, "''").replace(/[;\\]/g, "\\$0")
            return str.replace(/'/g, "''")
        }
    }
}


/*
 *  @copy   default
 *  
 *  Copyright (c) Embedthis Software LLC, 2003-2010. All Rights Reserved.
 *  Copyright (c) Michael O'Brien, 1993-2010. All Rights Reserved.
 *  
 *  This software is distributed under commercial and open source licenses.
 *  You may use the GPL open source license described below or you may acquire 
 *  a commercial license from Embedthis Software. You agree to be fully bound 
 *  by the terms of either license. Consult the LICENSE.TXT distributed with 
 *  this software for full details.
 *  
 *  This software is open source; you can redistribute it and/or modify it 
 *  under the terms of the GNU General Public License as published by the 
 *  Free Software Foundation; either version 2 of the License, or (at your 
 *  option) any later version. See the GNU General Public License for more 
 *  details at: http://www.embedthis.com/downloads/gplLicense.html
 *  
 *  This program is distributed WITHOUT ANY WARRANTY; without even the 
 *  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
 *  
 *  This GPL license does NOT permit incorporating this software into 
 *  proprietary programs. If you are unable to comply with the GPL, you must
 *  acquire a commercial license to use this software. Commercial licenses 
 *  for this software and support services are available from Embedthis 
 *  Software at http://www.embedthis.com 
 *  
 *  @end
 */
/************************************************************************/
/*
 *  End of file "../src/es/db/Database.es"
 */
/************************************************************************/



/************************************************************************/
/*
 *  Start of file "../src/es/db/DatabaseConnector.es"
 */
/************************************************************************/

/*
 *  DatabaseConnector.es -- Database Connector interface
 *
 *  Copyright (c) All Rights Reserved. See details at the end of the file.
 */

module ejs.db {

    /**
     *  Database interface connector. This interface is implemented by database connectors such as SQLite.
     *  @spec ejs
     *  @stability evolving
     */
    interface DatabaseConnector {

        use default namespace public

        // function DatabaseConnector(connectionString: String)

        /** @duplicate ejs.db::Database.addColumn */
        function addColumn(table: String, column: String, datatype: String, options: Object = null): Void

        /** @duplicate ejs.db::Database.addIndex */
        function addIndex(table: String, column: String, index: String): Void

        /** @duplicate ejs.db::Database.changeColumn */
        function changeColumn(table: String, column: String, datatype: String, options: Object = null): Void

        /** @duplicate ejs.db::Database.close */
        function close(): Void

        /** @duplicate ejs.db::Database.commit */
        function commit(): Void

        /** @duplicate ejs.db::Database.connect 
            Not yet implemented
            @hide
         */
        function connect(connectionString: String): Void

        /** @duplicate ejs.db::Database.createDatabase */
        function createDatabase(name: String, options: Object = null): Void

        /** @duplicate ejs.db::Database.createTable */
        function createTable(table: String, columns: Array = null): Void

        /** @duplicate ejs.db::Database.dataTypeToSqlType */
        function dataTypeToSqlType(dataType:String): String

        /** @duplicate ejs.db::Database.destroyDatabase */
        function destroyDatabase(name: String): Void

        /** @duplicate ejs.db::Database.destroyTable */
        function destroyTable(table: String): Void

        /** @duplicate ejs.db::Database.getColumns */
        function getColumns(table: String): Array

        /** @duplicate ejs.db::Database.getTables */
        function getTables(): Array

        /** @duplicate ejs.db::Database.removeColumns */
        function removeColumns(table: String, columns: Array): Void 

        /** @duplicate ejs.db::Database.removeIndex */
        function removeIndex(table: String, index: String): Void

        /** @duplicate ejs.db::Database.renameColumn */
        function renameColumn(table: String, oldColumn: String, newColumn: String): Void

        /** @duplicate ejs.db::Database.renameTable */
        function renameTable(oldTable: String, newTable: String): Void

        /** @duplicate ejs.db::Database.rollback 
            @hide
         */
        function rollback(): Void

        /** @duplicate ejs.db::Database.sql */
        function sql(cmd: String): Array

        /** @duplicate ejs.db::Database.sqlTypeToDataType */
        function sqlTypeToDataType(sqlType: String): String

        /** @duplicate ejs.db::Database.sqlTypeToEjsType */
        function sqlTypeToEjsType(sqlType: String): String

        /** @duplicate ejs.db::Database.startTransaction */
        function startTransaction(): Void
    }
}


/*
 *  @copy   default
 *  
 *  Copyright (c) Embedthis Software LLC, 2003-2010. All Rights Reserved.
 *  Copyright (c) Michael O'Brien, 1993-2010. All Rights Reserved.
 *  
 *  This software is distributed under commercial and open source licenses.
 *  You may use the GPL open source license described below or you may acquire 
 *  a commercial license from Embedthis Software. You agree to be fully bound 
 *  by the terms of either license. Consult the LICENSE.TXT distributed with 
 *  this software for full details.
 *  
 *  This software is open source; you can redistribute it and/or modify it 
 *  under the terms of the GNU General Public License as published by the 
 *  Free Software Foundation; either version 2 of the License, or (at your 
 *  option) any later version. See the GNU General Public License for more 
 *  details at: http://www.embedthis.com/downloads/gplLicense.html
 *  
 *  This program is distributed WITHOUT ANY WARRANTY; without even the 
 *  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
 *  
 *  This GPL license does NOT permit incorporating this software into 
 *  proprietary programs. If you are unable to comply with the GPL, you must
 *  acquire a commercial license to use this software. Commercial licenses 
 *  for this software and support services are available from Embedthis 
 *  Software at http://www.embedthis.com 
 *  
 *  @end
 */
/************************************************************************/
/*
 *  End of file "../src/es/db/DatabaseConnector.es"
 */
/************************************************************************/



/************************************************************************/
/*
 *  Start of file "../src/es/db/Record.es"
 */
/************************************************************************/

/**
    Record.es -- Record class
 *
    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/*
    Notes:
    - Don't use getters/setters in this module. This avoids collisions with record fields. Even with namespaces, 
      this can be a conceptual problem for users.
 */

module ejs.db {
    /**
        Database record class. A record instance corresponds to a row in the database. This class provides a low level 
        Object Relational Mapping (ORM) between the database and Ejscript objects. This class provides methods to create,
        read, update and delete rows in the database. When read or initialized object properties are dynamically created 
        in the Record instance for each column in the database table. Users should subclass the Record class for each 
        database table to manage.
        @spec ejs
        @stability evolving
     */
    class Record {

        use default namespace "ejs.db"
        use namespace "ejs.db.int"
        
        /** @hide */
        static "ejs.db.int" var  _assocName: String        //  Name for use in associations. Lower case class name
        /** @hide */
        static "ejs.db.int" var  _belongsTo: Array         //  List of belonging associations
        /** @hide */
        static "ejs.db.int" var  _className: String        //  Model class name
        /** @hide */
        static "ejs.db.int" var  _columns: Object          //  List of columns in this database table
        /** @hide */
        static "ejs.db.int" var  _hasOne: Array            //  List of 1-1 containment associations
        /** @hide */
        static "ejs.db.int" var  _hasMany: Array           //  List of 1-many containment  associations

        /** @hide */
        static "ejs.db.int" var  _db: Database             //  Hosting database
        /** @hide */
        static "ejs.db.int" var  _foreignId: String        //  Camel case class name with "Id". (userCartId))
        /** @hide */
        static "ejs.db.int" var  _keyName: String          //  Name of the key column (typically "id")
        /** @hide */
        static "ejs.db.int" var  _tableName: String        //  Name of the database table. Plural, PascalCase
        /** @hide */
        static "ejs.db.int" var  _trace: Boolean           //  Trace database SQL statements
        /** @hide */
        static "ejs.db.int" var  _validations: Array

        /** @hide */
        static "ejs.db.int" var  _beforeFilters: Array     //  Filters that run before saving data
        /** @hide */
        static "ejs.db.int" var  _afterFilters: Array      //  Filters that run after saving data
        /** @hide */
        static "ejs.db.int" var  _wrapFilters: Array       //  Filters that run before and after saving data

        /** @hide */
        "ejs.db.int" var _keyValue: Object                 //  Record key column value
        /** @hide */
        "ejs.db.int" var _errors: Object                   //  Error message aggregation
        /** @hide */
        "ejs.db.int" var _cacheAssoc: Object               //  Cached association data

        /** @hide */
        static var ErrorMessages = {
            accepted: "must be accepted",
            blank: "can't be blank",
            confirmation: "doesn't match confirmation",
            empty: "can't be empty",
            invalid: "is invalid",
            missing: "is missing",
            notNumber: "is not a number",
            notUnique: "is not unique",
            taken: "already taken",
            tooLong: "is too long",
            tooShort: "is too short",
            wrongLength: "wrong length",
            wrongFormat: "wrong format",
        }

        /*
            Initialize the model. This should be called by the model as its very first call.
         */
        _keyName = "id"
        _className = Reflect(this).name
        _assocName = _className.toCamel()
        _foreignId = _className.toCamel() + _keyName.toPascal()
        _tableName = plural(_className).toPascal()

        /**
            Construct a new record instance. This is really a constructor function, because the Record class is 
            implemented by user models, no constructor will be invoked when a new user model is instantiated. 
            The record may be initialized by optionally supplying field data. However, the record will not be 
            written to the database until $save is called. To read data from the database into the record, use 
            one of the $find methods.
            @param fields An optional object set of field names and values may be supplied to initialize the record.
         */
        function Record(fields: Object = null) {
            initialize(fields)
        }

        /** @hide */
        function initialize(fields: Object = null): Void {
            if (fields) for (let field in fields) {
                this."public"::[field] = fields[field]
            }
        }

        /**
            Run filters after saving data
            @param fn Function to run
            @param options - reserved
         */
        static function afterFilter(fn, options: Object = null): Void {
            _afterFilters ||= []
            _afterFilters.append([fn, options])
        }

        /**
            Run filters before saving data
            @param fn Function to run
            @param options - reserved
         */
        static function beforeFilter(fn, options: Object = null): Void {
            _beforeFilters ||= []
            _beforeFilters.append([fn, options])
        }

        /**
            Define a belonging reference to another model class. When a model belongs to another, it has a foreign key
            reference to another class.
            @param owner Referenced model class that logically owns this model.
            @param options Optional options hash
            @option className Name of the class
            @option foreignKey Key name for the foreign key
            @option conditions SQL conditions for the relationship to be satisfied
         */
        static function belongsTo(owner, options: Object = null): Void {
            _belongsTo ||= []
            _belongsTo.append(owner)
        }

        /*
            Read a single record of kind "model" by the given "key". Data is cached for subsequent reuse.
            Read into rec[field] from table[key]
         */
        private static function cachedRead(rec: Record, field: String, model, key: String, options: Object): Object {
            rec._cacheAssoc ||= {}
            if (rec._cacheAssoc[field] == null) {
                rec._cacheAssoc[field] =  model.readRecords(key, options); 
            }
            return rec._cacheAssoc[field]
        }

        private static function checkFormat(thisObj: Record, field: String, value, options: Object): Void {
            if (! RegExp(options.format).test(value)) {
                thisObj._errors[field] = options.message ? options.message : ErrorMessages.wrongFormat
            }
        }

        private static function checkNumber(thisObj: Record, field: String, value, options): Void {
            if (! RegExp(/^[0-9]+$/).test(value)) {
                thisObj._errors[field] = options.message ? options.message : ErrorMessages.notNumber
            }
        }

        private static function checkPresent(thisObj: Record, field: String, value, options): Void {
            if (value == undefined) {
                thisObj._errors[field] = options.message ? options.message : ErrorMessages.missing
            } else if (value.length == 0 || value.trim() == "" && thisObj._errors[field] == undefined) {
                thisObj._errors[field] = ErrorMessages.blank
            }
        }

        private static function checkUnique(thisObj: Record, field: String, value, options): Void {
            let grid: Array
            if (thisObj._keyValue) {
                grid = findWhere(field + ' = "' + value + '" AND id <> ' + thisObj._keyValue)
            } else {
                grid = findWhere(field + ' = "' + value + '"')
            }
            if (grid.length > 0) {
                thisObj._errors[field] = options.message ? options.message : ErrorMessages.notUnique
            }
        }

        /*
            Map types from SQL to ejs when reading from the database
         */
        private function coerceToEjsTypes(): Void {
            for (let field: String in this) {
                let col: Column = _columns[field]
                if (col == undefined) {
                    continue
                }
                if (col.ejsType == Reflect(this[field]).type) {
                    continue
                }
                let value: String = this[field]
                switch (col.ejsType) {
                case Boolean:
                    if (value is String) {
                        this[field] = (value.trim().toLower() == "true")
                    } else if (value is Number) {
                        this[field] = (value == 1)
                    } else {
                        this[field] = value cast Boolean
                    }
                    this[field] = (this[field]) ? true : false
                    break

                case Date:
                    this[field] = new Date(value)
                    break

                case Number:
                    this[field] = this[field] cast Number
                    break
                }
            }
        }

        /*
            Create associations for a record
         */
        private static function createAssociations(rec: Record, set: Array, preload, options): Void {
            for each (let model in set) {
                if (model is Array) model = model[0]
                // print("   Create Assoc for " + _tableName + "[" + model._assocName + "] for " + model._tableName + "[" + 
                //    rec[model._foreignId] + "]")
                if (preload == true || (preload && preload.contains(model))) {
                    /*
                        Query did table join, so rec already has the data. Extract the fields for the referred model and
                        then remove from rec and replace with an association reference. 
                     */
                    let association = {}
                    if (!model._columns) model.getSchema()
                    for (let field: String in model._columns) {
                        let f: String = "_" + model._className + field.toPascal()
                        association[field] = rec[f]
                        delete rec.public::[f]
                    }
                    rec[model._assocName] = model.createRecord(association, options)

                } else {
                    rec[model._assocName] = makeLazyReader(rec, model._assocName, model, rec[model._foreignId])
                    if (!model._columns) model.getSchema()
                    for (let field: String  in model._columns) {
                        let f: String = "_" + model._className + field.toPascal()
                        if (rec[f]) {
                            delete rec.public::[f]
                        }
                    }
                }
            }
        }

        /*
            Create a new record instance and apply the row data
            Process a sql result and add properties for each field in the row
         */
        private static function createRecord(data: Object, options: Object = {}) {
            let rec: Record = new global[_className]
            rec.initialize(data)
            rec._keyValue = data[_keyName]

            let subOptions = {}
            if (options.depth) {
                subOptions.depth = options.depth
                subOptions.depth--
            }

            if (options.include) {
                createAssociations(rec, options.include, true, subOptions)
            }
            if (options.depth != 0) {
                if (_belongsTo) {
                    createAssociations(rec, _belongsTo, options.preload, subOptions)
                }
                if (_hasOne) {
                    for each (model in _hasOne) {
                        if (!rec[model._assocName]) {
                            rec[model._assocName] = makeLazyReader(rec, model._assocName, model, null,
                                {conditions: rec._foreignId + " = " + data[_keyName] + " LIMIT 1"})
                        }
                    }
                }
                if (_hasMany) {
                    for each (model in _hasMany) {
                        if (!rec[model._assocName]) {
                            rec[model._assocName] = makeLazyReader(rec, model._assocName, model, null,
                                {conditions: rec._foreignId + " = " + data[_keyName]})
                        }
                    }
                }
            }
            rec.coerceToEjsTypes()
            return rec
        }

        /**
            Set an error message. This defines an error message for the given field in a record.
            @param field Name of the field to associate with the error message
            @param msg Error message
         */
        function error(field: String, msg: String): Void {
            field ||= ""
            _errors[field] = msg
        }

        /**
            Find a record. Find and return a record identified by its primary key if supplied or by the specified options. 
            If more than one record matches, return the first matching record.
            @param key Key Optional key value. Set to null if selecting via the options 
            @param options Optional search option values
            @returns a model record or null if the record cannot be found.
            @throws IOError on internal SQL errors
            @option columns List of columns to retrieve
            @option conditions { field: value, ...}   or [ "SQL condition", "id == 23", ...]
            @option from Low level from clause (not fully implemented)
            @option keys [set of matching key values]
            @option order ORDER BY clause
            @option group GROUP BY clause
            @option include [Model, ...] Models to join in the query and create associations for. Always preloads.
            @option joins Low level join statement "LEFT JOIN vists on stockId = visits.id". Low level joins do not
                create association objects (or lazy loaders). The names of the joined columns are prefixed with the
                appropriate table name using camel case (tableColumn).
            @option limit LIMIT count
            @option depth Specify the depth for which to create associations for belongsTo, hasOne and hasMany relationships.
                 Depth of 1 creates associations only in the immediate fields of the result. Depth == 2 creates in the 
                 next level and so on. Defaults to one.
            @option offset OFFSET count
            @option preload [Model1, ...] Preload "belongsTo" model associations rather than creating lazy loaders. This can
                reduce the number of database queries if iterating through associations.
            @option readonly
            @option lock
         */
        static function find(key: Object, options: Object = {}): Object {
            let grid: Array = innerFind(key, 1, options)
            if (grid.length >= 1) {
                let results = createRecord(grid[0], options)
                if (options && options.debug) {
                    print("RESULTS: " + serialize(results))
                }
                return results
            } 
            return null
        }

        /**
            Find all the matching records
            @param options Optional set of options. See $find for list of possible options.
            @returns An array of model records. The array may be empty if no matching records are found
            @throws IOError on internal SQL errors
         */
        static function findAll(options: Object = {}): Array {
            let grid: Array = innerFind(null, null, options)
            // start = new Date
            for (let i = 0; i < grid.length; i++) {
                grid[i] = createRecord(grid[i], options)
            }
            // print("findAll - create records TIME: " + start.elapsed())
            if (options && options.debug) {
                print("RESULTS: " + serialize(grid))
            }
            return grid
        }

        /**
            Find the first record matching a condition. Select a record using a given SQL where clause.
            @param where SQL WHERE clause to use when selecting rows.
            @returns a model record or null if the record cannot be found.
            @throws IOError on internal SQL errors
            @example
                rec = findOneWhere("cost < 200")
         */
        static function findOneWhere(where: String): Object {
            let grid: Array = innerFind(null, 1, { conditions: [where]})
            if (grid.length >= 1) {
                return createRecord(grid[0])
            } 
            return null
        }

        /**
            Find records matching a condition. Select a set of records using a given SQL where clause
            @param where SQL WHERE clause to use when selecting rows.
            @returns An array of objects. Each object represents a matching row with fields for each column.
            @example
                list = findWhere("cost < 200")
         */
        static function findWhere(where: String, count: Number = null): Array {
            let grid: Array = innerFind(null, null, { conditions: [where]})
            for (i in grid.length) {
                grid[i] = createRecord(grid[i])
            }
            return grid
        }

        /**
            Return the column names for the table
            @returns an array containing the names of the database columns. This corresponds to the set of properties
                that will be created when a row is read using $find.
         */
        static function getColumnNames(): Array { 
            if (!_columns) getSchema()
            let result: Array = []
            for (let col: Column in _columns) {
                result.append(col)
            }
            return result
        }

        /**
            Return the column names for the record
            @returns an array containing the Pascal case names of the database columns. The names have the first letter
                capitalized. 
         */
        static function getColumnTitles(): Array { 
            if (!_columns) getSchema()
            let result: Array = []
            for (let col: Column in _columns) {
                result.append(col.toPascal())
            }
            return result
        }

        /** Get the type of a column
            @param field Name of the field to examine.
            @return A string with the data type of the column
         */
        static function getColumnType(field: String): String {
            if (!_columns) getSchema()
            return _db.sqlTypeToDataType(_columns[field].sqlType)
        }

        /**
            Get the database connection for this record class
            @returns Database instance object created via new $Database
         */
        static function getDb(): Database {
            if (!_db) {
                _db = Database.defaultDatabase
            }
            return _db
        }

        /**
            Get the errors for the record. 
            @return The error message collection for the record.  
         */
        function getErrors(): Array
            _errors

        /**
            Get the key name for this record
         */
        static function getKeyName(): String
            _keyName

        /**
            Return the number of rows in the table
         */
        static function getNumRows(): Number {
            if (!_columns) getSchema()
            let cmd: String = "SELECT COUNT(*) FROM " + _tableName + " WHERE " + _keyName + " <> '';"
            let grid: Array = _db.query(cmd, "numRows", _trace)
            return grid[0]["COUNT(*)"]
        }

        /*
            Read the table schema and return the column hash
         */
        private static function getSchema(): Void {
            if (!_db) {
                _db = Database.defaultDatabase
                if (!_db) {
                    throw new Error("Can't get schema, database connection has not yet been established")
                }
            }
            let sql: String = 'PRAGMA table_info("' + _tableName + '");'
            let grid: Array = _db.query(sql, "schema", _trace)
            _columns = {}
            for each (let row: Record in grid) {
                let name = row["name"]
                let sqlType = row["type"].toLower()
                let ejsType = mapSqlTypeToEjs(sqlType)
                _columns[name] = new Column(name, false, ejsType, sqlType)
            }
        }

        /**
            Get the associated name for this record
            @returns the database table name backing this record class. Normally this is simply a plural class name. 
         */
        static function getTableName(): String
            _tableName

        /**
            Define a containment relationship to another model class. When using "hasAndBelongsToMany" on another model, it 
            means that other models have a foreign key reference to this class and this class can "contain" many instances 
            of the other models.
            @param model Model. (not implemented).
            @param options Object hash of options.
            @option foreignKey Key name for the foreign key. (not implemented).
            @option through String Class name which mediates the many to many relationship. (not implemented).
            @option joinTable. (not implemented).
         */
        static function hasAndBelongsToMany(model: Object, options: Object = {}): Void {
            belongsTo(model, options)
            hasMany(model, options)
        }

        /**
            Check if the record has any errors.
            @return True if the record has errors.
         */
        function hasError(field: String = null): Boolean {
            if (field) {
                return (_errors && _errors[field])
            }
            if (_errors) {
                return _errors.length > 0
            }
            return false
        }

        /**
            Define a containment relationship to another model class. When using "hasMany" on another model, it means 
            that other model has a foreign key reference to this class and this class can "contain" many instances of 
            the other.
            @param model Model class that is contained by this class. 
            @param options Call options
            @option things Model object that is posessed by this. (not implemented)
            @option through String Class name which mediates the many to many relationship. (not implemented)
            @option foreignKey Key name for the foreign key. (not implemented)
         */
        static function hasMany(model: Object, options: Object = {}): Void {
            _hasMany ||= []
            _hasMany.append(model)
        }

        /**
            Define a containment relationship to another model class. When using "hasOne" on another model, 
            it means that other model has a foreign key reference to this class and this class can "contain" 
            only one instance of the other.
            @param model Model class that is contained by this class. 
            @option thing Model that is posessed by this. (not implemented).
            @option foreignKey Key name for the foreign key (not implemented).
            @option as String  (not implemented).
         */
        static function hasOne(model: Object, options: Object = null): Void {
            _hasOne ||= []
            _hasOne.append(model)
        }

        /*
            Common find implementation. See find/findAll for doc.
         */
        static private function innerFind(key: Object, limit: Number = null, options: Object = {}): Array {
            let cmd: String
            let columns: Array
            let from: String
            let conditions: String
            let where: String

            if (!_columns) getSchema()
            if (options == null) {
                options = {}
            }
            //  LEGACY 1.0.0-B2
            if (options.noassoc) {
                options.depth = 0
            }

            if (options.columns) {
                columns = options.columns
                /*
                    Qualify "id" so it won't clash when doing joins. If the "click" option is specified, must have an ID
                 */
                let index: Number = columns.indexOf("id")
                if (index >= 0) {
                    columns[index] = _tableName + ".id"
                } else if (!columns.contains(_tableName + ".id")) {
                    columns.insert(0, _tableName + ".id")
                }
            } else {
                columns =  "*"
            }

            conditions = ""
            from = ""
            where = false

            if (options.from) {
                from = options.from
            } else {
                from = _tableName
            }

            if (options.include) {
                let model
                if (options.include is Array) {
                    for each (entry in options.include) {
                        if (entry is Array) {
                            model = entry[0]
                            from += " LEFT OUTER JOIN " + model._tableName
                            from += " ON " + entry[1]
                        } else {
                            model = entry
                            from += " LEFT OUTER JOIN " + model._tableName
                        }
                    }
                } else {
                    model = options.include
                    from += " LEFT OUTER JOIN " + model._tableName
                    // conditions = " ON " + model._tableName + ".id = " + _tableName + "." + model._assocName + "Id"
                }
            }

            if (options.depth != 0) {
                if (_belongsTo) {
                    conditions = " ON "
                    for each (let owner in _belongsTo) {
                        from += " INNER JOIN " + owner._tableName
                    }
                    for each (let owner in _belongsTo) {
                        let tname: String = Reflect(owner).name
                        tname = tname[0].toLower() + tname.slice(1) + "Id"
                        conditions += _tableName + "." + tname + " = " + owner._tableName + "." + owner._keyName + " AND "
                    }
                    if (conditions == " ON ") {
                        conditions = ""
                    }
                }
            }

            if (options.joins) {
                if (conditions == "") {
                    conditions = " ON "
                }
                let parts: Array = options.joins.split(/ ON | on /)
                from += " " + parts[0]
                if (parts.length > 1) {
                    conditions += parts[1] + " AND "
                }
            }
            conditions = conditions.trim(" AND ")

            if (options.conditions) {
                let whereConditions: String = " WHERE "
                if (options.conditions is Array) {
                    for each (cond in options.conditions) {
                        whereConditions += cond + " " + " AND "
                    }
                    whereConditions = whereConditions.trim(" AND ")

                } else if (options.conditions is String) {
                    whereConditions += options.conditions + " " 

                } else if (options.conditions is Object) {
                    for (field in options.conditions) {
                        //  Remove quote from options.conditions[field]
                        whereConditions += field + " = '" + options.conditions[field] + "' " + " AND "
                    }
                }
                whereConditions = whereConditions.trim(" AND ")
                if (whereConditions != " WHERE ") {
                    where = true
                    conditions += whereConditions
                } else {
                    whereConditions = ""
                    from = from.trim(" AND ")
                }

            } else {
                from = from.trim(" AND ")
            }

            if (key || options.key) {
                if (!where) {
                    conditions += " WHERE "
                    where = true
                } else {
                    conditions += " AND "
                }
                conditions += (_tableName + "." + _keyName + " = ") + ((key) ? key : options.key)
            }

            //  Removed quote from "from"
            cmd = "SELECT " + Database.quote(columns) + " FROM " + from + conditions
            if (options.group) {
                cmd += " GROUP BY " + options.group
            }
            if (options.order) {
                cmd += " ORDER BY " + options.order
            }
            if (limit) {
                cmd += " LIMIT " + limit
            } else if (options.limit) {
                cmd += " LIMIT " + options.limit
            }
            if (options.offset) {
                cmd += " OFFSET " + options.offset
            }
            cmd += ";"

            if (_db == null) {
                throw new Error("Database connection has not yet been established")
            }

            let results: Array
            try {
                if (_trace || 1) {
                    results = _db.query(cmd, "find", _trace)
                } else {
                    results = _db.query(cmd, "find", _trace)
                }
            }
            catch (e) {
                throw e
            }
            return results
        }

        /*
            Make a getter function to lazily (on-demand) read associated records (belongsTo)
         */
        private static function makeLazyReader(rec: Record, field: String, model, key: String, 
                options: Object = {}): Function {
            // print("Make lazy reader for " + _tableName + "[" + field + "] for " + model._tableName + "[" + key + "]")
            var lazyReader: Function = function(): Object {
                // print("Run reader for " + _tableName + "[" + field + "] for " + model._tableName + "[" + key + "]")
                return cachedRead(rec, field, model, key, options)
            }
            return makeGetter(lazyReader)
        }

        private static function mapSqlTypeToEjs(sqlType: String): String {
            sqlType = sqlType.replace(/\(.*/, "")
            let ejsType: String = _db.sqlTypeToEjsType(sqlType)
            if (ejsType == undefined) {
                throw new Error("Unsupported SQL type: \"" + sqlType + "\"")
            }
            return ejsType
        }

        /*
            Prepare a value to be written to the database
         */
        private static function prepareValue(field: String, value: Object): String {
            let col: Column = _columns[field]
            if (col == undefined) {
                return undefined
            }
			if (value == undefined) {
				throw new Error("Field \"" + field + "\" is undefined")
			}
			if (value == null) {
				throw new Error("Field \"" + field + "\" is null")
			}
            switch (col.ejsType) {
            case Boolean:
                if (value is String) {
                    value = (value.toLower() == "true")
                } else if (value is Number) {
                    value = (value == 1)
                } else {
                    value = value cast Boolean
                }
                return value

            case Date:
                return "%Ld".format((new Date(value)).time)

            case Number:
                return value cast Number
                // return "%Ld".format(value cast Number)
             
            case String:
                return Database.quote(value)
            }
            return Database.quote(value.toString())
        }

        /*
            Read records for an assocation. Will return one or an array of records matching the supplied key and options.
         */
        private static function readRecords(key: String, options: Object): Object {
            let data: Array = innerFind(key, null, options)
            if (data.length > 1) {
                let result: Array = new Array
                for each (row in data) {
                    result.append(createRecord(row))
                }
                return result

            } else if (data.length == 1) {
                return createRecord(data[0])
            }
            return null
        }

        /**
            Remove records from the database
            @param keys Set of keys identifying the records to remove
         */
        static function remove(...keys): Void {
            for each (let key: Object in keys) {
                let cmd: String = "DELETE FROM " + _tableName + " WHERE " + _keyName + " = " + key + ";"
                db.query(cmd, "remove", _trace)
            }
        }

        private function runFilters(filters): Void {
            for each (filter in filters) {
                let fn = filter[0]
                let options = filter[1]
                if (options) {
                    let only = options.only
                }
                fn.call(this)
            }
        }

        /**
            Save the record to the database.
            @returns True if the record is validated and successfully written to the database
            @throws IOError Throws exception on sql errors
         */
        function save(): Boolean {
            var sql: String
            if (!_columns) getSchema()
            if (!validateRecord()) {
                return false
            }
            runFilters(_beforeFilters)
            
            if (_keyValue == null) {
                sql = "INSERT INTO " + _tableName + " ("
                for (let field: String in this) {
                    if (_columns[field]) {
                        sql += field + ", "
                    }
                }
                sql = sql.trim(', ')
                sql += ") VALUES("
                for (let field: String in this) {
                    if (_columns[field]) {
                        sql += "'" + prepareValue(field, this[field]) + "', "
                    }
                }
                sql = sql.trim(', ')
                sql += ")"

            } else {
                sql = "UPDATE " + _tableName + " SET "
                for (let field: String in this) {
                    if (_columns[field]) {
                        sql += field + " = '" + prepareValue(field, this[field]) + "', "
                    }
                }
                sql = sql.trim(', ')
                sql += " WHERE " + _keyName + " = " +  _keyValue
            }
            if (!_keyValue) {
                sql += "; SELECT last_insert_rowid();"
            } else {
                sql += ";"
            }

            let result: Array = _db.query(sql, "save", _trace)
            if (!_keyValue) {
                _keyValue = this["id"] = result[0]["last_insert_rowid()"] cast Number
            }
            runFilters(_afterFilters)
            return true
        }

        /**
            Update a record based on the supplied fields and values.
            @param fields Hash of field/value pairs to use for the record update.
            @returns True if the database is successfully updated. Returns false if validation fails. In this case,
                the record is not saved.
            @throws IOError on database SQL errors
         */
        function saveUpdate(fields: Object): Boolean {
            for (field in fields) {
                if (this[field] != undefined) {
                    this[field] = fields[field]
                }
            }
            return save()
        }

        /**
            Set the database connection for this record class
            @param database Database instance object created via new $Database
         */
        static function setDb(database: Database) {
            _db = database
        }

        /**
            Set the key name for this record
         */
        static function setKeyName(name: String): Void {
            _keyName = name
        }

        /**
            Set the associated table name for this record
            @param name Name of the database table to backup the record class.
         */
        static function setTableName(name: String): Void {
            if (_tableName != name) {
                _tableName = name
                if (_db) {
                    getSchema()
                }
            }
        }

        /**
            Run an SQL statement and return selected records.
            @param cmd SQL command to issue
            @returns An array of objects. Each object represents a matching row with fields for each column.
         */
        static function sql(cmd: String, count: Number = null): Array {
            cmd = "SELECT " + cmd + ";"
            return db.query(cmd, "select", _trace)
        }

        /**
            Trace SQL statements. Control whether trace is enabled for the actual SQL statements issued against the database.
            @param on If true, display each SQL statement to the log
         */
        static function trace(on: Boolean): void
            _trace = on

        /** @hide */
        static function validateFormat(fields: Object, options: Object = null) {
            if (_validations == null) {
                _validations = []
            }
            _validations.append([checkFormat, fields, options])
        }

        /** @hide */
        static function validateNumber(fields: Object, options: Object = null) {
            if (_validations == null) {
                _validations = []
            }
            _validations.append([checkNumber, fields, options])
        }

        /** @hide */
        static function validatePresence(fields: Object, options: Object = null) {
            if (_validations == null) {
                _validations = []
            }
            _validations.append([checkPresent, fields, options])
        }

        /**
            Validate a record. This call validates all the fields in a record.
            @returns True if the record has no errors.
         */
        function validateRecord(): Boolean {
            if (!_columns) getSchema()
            _errors = {}
            if (_validations) {
                for each (let validation: String in _validations) {
                    let check = validation[0]
                    let fields = validation[1]
                    let options = validation[2]
                    if (fields is Array) {
                        for each (let field in fields) {
                            if (_errors[field]) {
                                continue
                            }
                            check(this, field, this[field], options)
                        }
                    } else {
                        check(this, fields, this[fields], options)
                    }
                }
            }
            let thisType = Reflect(this).type
            if (thisType["validate"]) {
                thisType["validate"].call(this)
            }
            if (_errors.length == 0) {
                coerceToEjsTypes()
            }
            return _errors.length == 0
        }

        /** @hide */
        static function validateUnique(fields: Object, options: Object = null)
            _validations.append([checkUnique, fields, options])

        /**
            Run filters before and after saving data
            @param fn Function to run
            @param options - reserved
         */
        static function wrapFilter(fn, options: Object = null): Void {
            _wrapFilters ||= []
            _wrapFilters.append([fn, options])
        }

        //  LEGACY DEPRECATED in 1.0.0-B2
        /** @hide */
        static function get columnNames(): Array {
            return getColumnNames()
        }
        /** @hide */
        static function get columnTitles(): Array {
            return getColumnTitles()
        }
        /** @hide */
        static function get db(): Datbase {
            return getDb()
        }
        /** @hide */
        static function get keyName(): String {
            return getKeyName()
        }
        /** @hide */
        static function get numRows(): String {
            return getNumRows()
        }
        /** @hide */
        static function get tableName(): String {
            return getTableName()
        }
        /** @hide */
        function constructor(fields: Object = null): Void {
            initialize(fields)
        }
    }


    /**
        Database column class. A database record is comprised of one or mor columns
        @hide
     */
    class Column {
        public var ejsType: Object 
        public var sqlType: Object 

        function Column(name: String, accessor: Boolean = false, ejsType: Type = null, sqlType: String = null) {
            this.ejsType = ejsType
            this.sqlType = sqlType
        }
    }

    /** @hide */
    function plural(name: String): String
        name + "s"

    /** @hide */
    function singular(name: String): String {
        var s: String = name + "s"
        if (name.endsWith("ies")) {
            return name.slice(0,-3) + "y"
        } else if (name.endsWith("es")) {
            return name.slice(0,-2)
        } else if (name.endsWith("s")) {
            return name.slice(0,-1)
        }
        return name.toPascal()
    }

    /**
        Map a type assuming it is already of the correct ejs type for the schema
        @hide
     */
    function mapType(value: Object): String {
        if (value is Date) {
            return "%Ld".format((new Date(value)).time)
        } else if (value is Number) {
            return "%Ld".format(value)
        }
        return value
    }
}


/*
    @copy   default
    
    Copyright (c) Embedthis Software LLC, 2003-2010. All Rights Reserved.
    Copyright (c) Michael O'Brien, 1993-2010. All Rights Reserved.
    
    This software is distributed under commercial and open source licenses.
    You may use the GPL open source license described below or you may acquire 
    a commercial license from Embedthis Software. You agree to be fully bound 
    by the terms of either license. Consult the LICENSE.TXT distributed with 
    this software for full details.
    
    This software is open source; you can redistribute it and/or modify it 
    under the terms of the GNU General Public License as published by the 
    Free Software Foundation; either version 2 of the License, or (at your 
    option) any later version. See the GNU General Public License for more 
    details at: http://www.embedthis.com/downloads/gplLicense.html
    
    This program is distributed WITHOUT ANY WARRANTY; without even the 
    implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
    
    This GPL license does NOT permit incorporating this software into 
    proprietary programs. If you are unable to comply with the GPL, you must
    acquire a commercial license to use this software. Commercial licenses 
    for this software and support services are available from Embedthis 
    Software at http://www.embedthis.com 
    
    @end
 */
/************************************************************************/
/*
 *  End of file "../src/es/db/Record.es"
 */
/************************************************************************/



/************************************************************************/
/*
 *  Start of file "../src/es/db/Sqlite.es"
 */
/************************************************************************/

/**
 *  Sqlite.es -- SQLite Database class
 *
 *  Copyright (c) All Rights Reserved. See details at the end of the file.
 */

module ejs.db.sqlite {

    /**
     *  SQLite database support
     *  @spec ejs
     *  @stability evolving
     */
    "ejs.db" class Sqlite {

        /*
         *  Map independent types to SQL types
         */
        static const DataTypeToSqlType: Object = {
            "binary":       "blob",
            "boolean":      "tinyint",
            "date":         "date",
            "datetime":     "datetime",
            "decimal":      "decimal",
            "float":        "float",
            "integer":      "int",
            "number":       "decimal",
            "string":       "varchar",
            "text":         "text",
            "time":         "time",
            "timestamp":    "datetime",
        }

        /*
         *  Map independent types to SQL types
         */
        static const SqlTypeToDataType: Object = {
            "blob":         "binary",
            "tinyint":      "boolean",
            "date":         "date",
            "datetime":     "datetime",
            "decimal":      "decimal",
            "float":        "float",
            "int":          "integer",
            "varchar":      "string",
            "text":         "text",
            "time":         "time",
        }

        /*
         *  Map SQL types to Ejscript native types
         */
        static const SqlTypeToEjsType: Object = {
            "blob":         String,
            "date":         Date,
            "datetime":     Date,
            "decimal":      Number,
            "int":          Number,
            "integer":      Number,
            "float":        Number,
            "time":         Date,
            "tinyint":      Boolean,
            "text":         String,
            "varchar":      String,
        }

        /*
            Map Ejscript native types back to SQL types
            INCOMPLETE and INCORRECT
         
        static const EjsToDataType: Object = {
            "string":       "varchar",
            "number":       "decimal",
            "date":         "datetime",
            "bytearray":    "Blob",
            "boolean":      "tinyint",
        }
         */

        use default namespace public

        /**
            Initialize a database connection using the supplied database connection string
            @param connectionString Connection string stipulating how to connect to the database. The only supported 
            format is:
                <ul>
                    <li>filename</li>
                </ul>
                Where filename is the path to the database. 
         */
        native "ejs.db" function Sqlite(connectionString: String)

        /** @duplicate ejs.db::Database.addColumn */
        function addColumn(table: String, column: String, datatype: String, options: Object = null): Void {
            datatype = DataTypeToSqlType[datatype.toLower()]
            if (datatype == undefined) {
                throw "Bad Ejscript column type: " + datatype
            }
            query("ALTER TABLE " + table + " ADD " + column + " " + datatype)
        }

        /** @duplicate ejs.db::Database.addIndex */
        function addIndex(table: String, column: String, index: String): Void
            query("CREATE INDEX " + index + " ON " + table + " (" + column + ");")

        /** 
            @duplicate ejs.db::Database.changeColumn 
            @hide
            SQLite cannot change or rename columns.
         */
        function changeColumn(table: String, column: String, datatype: String, options: Object = null): Void {
            datatype = datatype.toLower()
            if (DataTypeToSqlType[datatype] == undefined) {
                throw "Bad column type: " + datatype
            }
            /* query("ALTER TABLE " + table + " CHANGE " + column + " " + datatype) */
            throw "SQLite does not support column changes"
        }

        /** @duplicate ejs.db::Database.close */
        native function close(): Void

        /** @duplicate ejs.db::Database.commit  Not implemented */
        function commit(): Void {}

        /** @duplicate ejs.db::Database.connect 
            Not yet implemented.
            @hide
         */
        native function connect(connectionString: String): Void

        /** @duplicate ejs.db::Database.createDatabase */
        function createDatabase(name: String, options: Object = null): Void {
            /* Nothing to do for sqlite */
        }

        /** @duplicate ejs.db::Database.createTable */
        function createTable(table: String, columns: Array = null): Void {
            let cmd: String

            query("DROP TABLE IF EXISTS " + table + ";")
            query("CREATE TABLE " + table + "(id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL);")

            if (columns) {
                for each (let colspec: String in columns) {
                    let spec: Array = colspec.split(":")
                    if (spec.length != 2) {
                        throw "Bad column spec: " + spec
                    }
                    let column: String = spec[0]
                    let datatype: String = spec[1]
                    addColumn(table, column.trim(), datatype.trim())
                }
            }
        }

        /** @duplicate ejs.db::Database.dataTypeToSqlType */
        static function dataTypeToSqlType(dataType:String): String
            DataTypeToSqlType[dataType]

        /** @duplicate ejs.db::Database.destroyDatabase */
        function destroyDatabase(name: String): Void
            Path(name).remove()

        /** @duplicate ejs.db::Database.destroyTable */
        function destroyTable(table: String): Void
            query("DROP TABLE IF EXISTS " + table + ";")

        /** @duplicate ejs.db::Database.endTransaction */
        function endTransaction(): Void {}

        /** @duplicate ejs.db::Database.getColumns */
        function getColumns(table: String): Array {
            grid = query('PRAGMA table_info("' + table + '");')
            let names = []
            for each (let row in grid) {
                let name: String = row["name"]
                names.append(name)
            }
            return names
        }

        /**
         *  @duplicate ejs.db::Database.getNumRows
         */
        function getNumRows(table: String): Number {
            let cmd: String = "SELECT COUNT(*) FROM " + table + ";"
            let grid: Array = query(cmd, "numRows")
            return grid[0]["COUNT(*)"] cast Number
        }

        /** @duplicate ejs.db::Database.getTables */
        function getTables(): Array {
            let cmd: String = "SELECT name from sqlite_master WHERE type = 'table' order by NAME;"
            let grid: Array = query(cmd)
            let result: Array = new Array
            for each (let row: Object in grid) {
                let name: String = row["name"]
                if (!name.contains("sqlite_") && !name.contains("_Ejs")) {
                    result.append(row["name"])
                }
            }
            return result
        }

        /** @duplicate ejs.db::Database.removeColumns */
        function removeColumns(table: String, columns: Array): Void {
            /*
             *  This is a dumb SQLite work around because it doesn't have drop column
             */
            backup = "_backup_" + table
            keep = getColumns(table)
            for each (column in columns) {
                if ((index = keep.indexOf(column)) < 0) {
                    throw "Column \"" + column + "\" does not exist in " + table
                } 
                keep.remove(index)
            }

            schema = 'PRAGMA table_info("' + table + '");'
            grid = query(schema)
            types = {}
            for each (let row in grid) {
                let name: String = row["name"]
                types[name] = row["type"]
            }

            columnSpec = []
            for each (k in keep) {
                if (k == "id") {
                    columnSpec.append(k + " INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL")
                } else {
                    columnSpec.append(k + " " + types[k])
                }
            }

            cmd = "BEGIN TRANSACTION;
                CREATE TEMPORARY TABLE " + backup + "(" + columnSpec + ");
                INSERT INTO " + backup + " SELECT " + keep + " FROM " + table + ";
                DROP TABLE " + table + ";
                CREATE TABLE " + table + "(" + columnSpec + ");
                INSERT INTO " + table + " SELECT " + keep + " FROM " + backup + ";
                DROP TABLE " + backup + ";
                COMMIT;"
            query(cmd)
        }

        /** @duplicate ejs.db::Database.removeIndex */
        function removeIndex(table: String, index: String): Void
            query("DROP INDEX " + index + ";")

        /** 
            @duplicate ejs.db::Database.renameColumn 
            @hide
            SQLite does not support renaming columns
         */
        function renameColumn(table: String, oldColumn: String, newColumn: String): Void {
            throw "SQLite does not support renaming columns"
            // query("ALTER TABLE " + table + " RENAME " + oldColumn + " TO " + newColumn + ";")
        }

        /** @duplicate ejs.db::Database.renameTable */
        function renameTable(oldTable: String, newTable: String): Void
            query("ALTER TABLE " + oldTable + " RENAME TO " + newTable + ";")

        /** @duplicate ejs.db::Database.rollback 
            @hide
         */
        function rollback(): Void {}

        /** @duplicate ejs.db::Database.query */
        function query(cmd: String, tag: String = "SQL", trace: Boolean = false): Array {
            if (trace) {
                print(tag + ": " + cmd)
            }
            return sql(cmd)
        }

        /** @duplicate ejs.db::Database.sql */
        native function sql(cmd: String): Array

        /** @duplicate ejs.db::Database.sqlTypeToDataType */
        static function sqlTypeToDataType(sqlType: String): String
            SqlTypeToDataType[sqlType]

        /** @duplicate ejs.db::Database.sqlTypeToEjsType */
        static function sqlTypeToEjsType(sqlType: String): Type
            SqlTypeToEjsType[sqlType]

        /** @duplicate ejs.db::Database.startTransaction */
        function startTransaction(): Void {}

    }
}


/*
 *  @copy   default
 *  
 *  Copyright (c) Embedthis Software LLC, 2003-2010. All Rights Reserved.
 *  Copyright (c) Michael O'Brien, 1993-2010. All Rights Reserved.
 *  
 *  This software is distributed under commercial and open source licenses.
 *  You may use the GPL open source license described below or you may acquire 
 *  a commercial license from Embedthis Software. You agree to be fully bound 
 *  by the terms of either license. Consult the LICENSE.TXT distributed with 
 *  this software for full details.
 *  
 *  This software is open source; you can redistribute it and/or modify it 
 *  under the terms of the GNU General Public License as published by the 
 *  Free Software Foundation; either version 2 of the License, or (at your 
 *  option) any later version. See the GNU General Public License for more 
 *  details at: http://www.embedthis.com/downloads/gplLicense.html
 *  
 *  This program is distributed WITHOUT ANY WARRANTY; without even the 
 *  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
 *  
 *  This GPL license does NOT permit incorporating this software into 
 *  proprietary programs. If you are unable to comply with the GPL, you must
 *  acquire a commercial license to use this software. Commercial licenses 
 *  for this software and support services are available from Embedthis 
 *  Software at http://www.embedthis.com 
 *  
 *  @end
 */
/************************************************************************/
/*
 *  End of file "../src/es/db/Sqlite.es"
 */
/************************************************************************/



/************************************************************************/
/*
 *  Start of file "../src/es/events/Dispatcher.es"
 */
/************************************************************************/

/*
    Dispatcher.es -- Event dispatcher target.
 *
    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

module ejs.events {

    /**
        The Dispatcher class supports the registration of listeners who want notification of events of interest.
        WARNING: This class is prototype and it not currently supported.

        @example
            class Shape {
                public var events: Dispatcher = new Dispatcher
            }
            function callback(e: Event) {
                //  Do something
            }
            var s : Shape = new Shape
            s.events.addListener(callback)
            s.events.dispatch(new Event("Great Event"))

            // Main Program then calls
            App.serviceEvents()
        @hide
     */
    class Dispatcher {
        var endpoints: Object

        use default namespace public

        /**
            Construct a new event Dispatcher object
         */
        function Dispatcher()
            endpoints = new Object

        /**
            Add a listener function for events.
            @param callback Function to call when the event is received.
            @param eventType Event class to listen for. The listener will receive events of this event class or any 
                of its subclasses. Defaults to Event.
         */
        function addListener(callback: Function, eventType: Type = Event): Void {
            var name = Reflect(eventType).name
            var listeners : Array

            listeners = endpoints[name]
            if (listeners == undefined) {
                listeners = endpoints[name] = new Array
            } else {
                for each (var e: Endpoint in listeners) {
                    if (e.callback == callback && e.eventType == eventType) {
                        return
                    }
                }
            }
            listeners.append(new Endpoint(callback, eventType))
        }

        /**
            Dispatch an event to the registered listeners. This is called by the class owning the event dispatcher.
            @param event Event instance to send to the listeners.
         */
        function dispatch(event: Event): Void {
            var listeners : Array
            var name = Reflect(event).typeName

            listeners = endpoints[name]
            if (listeners != undefined) {
                for each (var e: Endpoint in listeners) {
                    if (event is e.eventType) {
                        e.callback(event)
                    }
                }
            }
        }

        /**
            Remove a registered listener.
            @param eventType Event class used when adding the listener.
            @param callback Listener callback function used when adding the listener.
         */
        function removeListener(callback: Function, eventType: Type = Event): Void {
            var name = Reflect(eventType).name
            var listeners: Array

            listeners = endpoints[name]
            for (let i in listeners) {
                var e: Endpoint = listeners[i]
                if (e.callback == callback && e.eventType == eventType) {
                    listeners.remove(i, i + 1)
                }
            }
        }
    }


    /*
        Listening endpoints
        @hide
     */
    internal class Endpoint {
        public var callback: Function
        public var eventType: Type
        function Endpoint(callback: Function, eventType: Type) {
            this.callback = callback
            this.eventType = eventType
        }
    }
}


/*
    @copy   default
    
    Copyright (c) Embedthis Software LLC, 2003-2010. All Rights Reserved.
    Copyright (c) Michael O'Brien, 1993-2010. All Rights Reserved.
    
    This software is distributed under commercial and open source licenses.
    You may use the GPL open source license described below or you may acquire 
    a commercial license from Embedthis Software. You agree to be fully bound 
    by the terms of either license. Consult the LICENSE.TXT distributed with 
    this software for full details.
    
    This software is open source; you can redistribute it and/or modify it 
    under the terms of the GNU General Public License as published by the 
    Free Software Foundation; either version 2 of the License, or (at your 
    option) any later version. See the GNU General Public License for more 
    details at: http://www.embedthis.com/downloads/gplLicense.html
    
    This program is distributed WITHOUT ANY WARRANTY; without even the 
    implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
    
    This GPL license does NOT permit incorporating this software into 
    proprietary programs. If you are unable to comply with the GPL, you must
    acquire a commercial license to use this software. Commercial licenses 
    for this software and support services are available from Embedthis 
    Software at http://www.embedthis.com 
    
    @end
 */
/************************************************************************/
/*
 *  End of file "../src/es/events/Dispatcher.es"
 */
/************************************************************************/



/************************************************************************/
/*
 *  Start of file "../src/es/events/Event.es"
 */
/************************************************************************/

/*
    Event.es -- Event class
 *
    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

module ejs.events {

    /**
        WARNGING: This class is prototype and will be changed in the next release.
        The Event class encapsulates information pertaining to a system or application event. Applications typically
        subclass Event to add custom event data if required. Events are initiated via the EventTarget class and are
        routed to listening functions via a system event queue.
        @stability prototype
        @example 
            class UpdateEvent extends Event {
            }
            obj.events.dispatch(new UpdateEvent())
        @hide
     */
    class Event {

        use default namespace public

        /**
            Low priority constant for use with the Event() constructor method.
         */
        static const PRI_LOW: Number = 25;


        /**
            Normal priority constant for use with the Event() constructor method.
         */
        static const PRI_NORMAL: Number = 50;

        /**
            High priority constant for use with the Event() constructor method.
         */
        static const PRI_HIGH: Number = 75;

        /**
            Whether the event will bubble up to the listeners parent
         */
        var bubbles: Boolean

        /**
            Event data associated with the Event. When Events are created, the constructor optionally takes an arbitrary 
            object data reference.
         */
        var data: Object

        /**
            Time the event was created. The Event constructor will automatically set the timestamp to the current time.  
         */
        var timestamp: Date

        /**
            Event priority. Priorities are 0-99. Zero is the highest priority and 50 is normal. Use the priority 
            symbolic constants PRI_LOW, PRI_NORMAL or PRI_HIGH.
         */
        var priority: Number

        /**
            Constructor for Event. Create a new Event object.
            @param data Arbitrary object to associate with the event.
            @param bubbles Bubble the event to the listener's parent if true. Not currently implemented.
            @param priority Event priority.
         */
        function Event(data: Object = null, bubbles: Boolean = false, priority: Number = PRI_NORMAL) {
            this.timestamp = new Date
            this.data = data
            this.priority = priority
            this.bubbles = bubbles
        }

        /**
            Create a string representation of the event
            @return A string of the form: "[Event: EventName]"
         */
        override function toString(): String {
            return "[Event: " +  Reflect(this).typeName + "]"
        }
    }

    /**
        WARNGING: This class is prototype and will be changed in the next release.
        Event for exceptions
        @spec WebWorker
        @hide
     */
    class ErrorEvent extends Event {
        use default namespace public

        /**
            Event message
         */
        var message: String

        /**
            Source filename of the script that issued the error
         */
        var filename: String

        /**
            Source line number in the script that issued the error
         */
        var lineno: String

        /**
            Callback stack at the point of the error
         */
        var stack: String
    }
}


/*
    @copy   default
    
    Copyright (c) Embedthis Software LLC, 2003-2010. All Rights Reserved.
    Copyright (c) Michael O'Brien, 1993-2010. All Rights Reserved.
    
    This software is distributed under commercial and open source licenses.
    You may use the GPL open source license described below or you may acquire 
    a commercial license from Embedthis Software. You agree to be fully bound 
    by the terms of either license. Consult the LICENSE.TXT distributed with 
    this software for full details.
    
    This software is open source; you can redistribute it and/or modify it 
    under the terms of the GNU General Public License as published by the 
    Free Software Foundation; either version 2 of the License, or (at your 
    option) any later version. See the GNU General Public License for more 
    details at: http://www.embedthis.com/downloads/gplLicense.html
    
    This program is distributed WITHOUT ANY WARRANTY; without even the 
    implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
    
    This GPL license does NOT permit incorporating this software into 
    proprietary programs. If you are unable to comply with the GPL, you must
    acquire a commercial license to use this software. Commercial licenses 
    for this software and support services are available from Embedthis 
    Software at http://www.embedthis.com 
    
    @end
 */
/************************************************************************/
/*
 *  End of file "../src/es/events/Event.es"
 */
/************************************************************************/



/************************************************************************/
/*
 *  Start of file "../src/es/events/Timer.es"
 */
/************************************************************************/

/*
 *  Timer.es -- Timer Services
 *
 *  Copyright (c) All Rights Reserved. See details at the end of the file.
 */

module ejs.events {

    /**
     *  WARNING: This class is prototype and will be changed in the next release
     *  Timers manage the execution of functions at some point in the future. Timers run repeatedly until stopped by 
     *  calling the stop() method. Timers are scheduled with a granularity of 1 millisecond. However, many systems are not 
     *  capable of supporting this granularity and make only best efforts to schedule events at the desired time.
     *  To use timers, the application must call Dispatcher.serviceEvents.
     *  @Example
     *      function callback(e: Event) {
     *      }
     *      new Timer(200, callback)
     *  or
     *      new Timer(200, function (e) { print(e); })
     *  @hide
     */
    native class Timer {

        use default namespace public

        /**
         *  Constructor for Timer. The timer is will not be called until @start is called.
         *  @param callback Function to invoke when the timer is due.
         *  @param period Time period in milliseconds between invocations of the callback
         *  @param drift Set the timers drift setting. See @drift.
         */
        native function Timer(period: Number, callback: Function, drift: Boolean = true)

        /**
         *  The timer drift setting.
         *  If drift is false, reschedule the timer so that the time period between callback start times does not drift 
         *  and is best-efforts equal to the timer reschedule period. The timer subsystem will delay other low priority
         *  events or timers, with drift equal to true, if necessary to ensure non-drifting timers are scheduled exactly. 
         *  Setting drift to true will schedule the timer so that the time between the end of the callback and the 
         *  start of the next callback invocation is equal to the period. 
         */
        native function get drift(): Boolean

        /**
         *  @duplicate Timer.drift
         *  @param enable If true, allow the timer to drift
         */
        native function set drift(enable: Boolean): Void

        /**
         *  The timer interval period in milliseconds.
         */
        native function get period(): Number

        /**
         *  Set the timer period and reschedule the timer.
         *  @param period New time in milliseconds between timer invocations.
         */
        native function set period(period: Number): Void

        /**
         *  Restart a stopped timer. Once running, the callback function will be invoked every @period milliseconds 
         *  according to the @drift setting. If the timer is already stopped, this function has no effect
         */
        native function restart(): Void

        /**
         *  Stop a timer running. Once stopped a timer can be restarted by calling @start.
         */
        native function stop(): Void
    }


    /**
     *  WARNING: This class is prototype and will be changed in the next release
     *  Timer event
     *  @hide
     */
    class TimerEvent extends Event { }
}


/*
 *  @copy   default
 *  
 *  Copyright (c) Embedthis Software LLC, 2003-2010. All Rights Reserved.
 *  Copyright (c) Michael O'Brien, 1993-2010. All Rights Reserved.
 *  
 *  This software is distributed under commercial and open source licenses.
 *  You may use the GPL open source license described below or you may acquire 
 *  a commercial license from Embedthis Software. You agree to be fully bound 
 *  by the terms of either license. Consult the LICENSE.TXT distributed with 
 *  this software for full details.
 *  
 *  This software is open source; you can redistribute it and/or modify it 
 *  under the terms of the GNU General Public License as published by the 
 *  Free Software Foundation; either version 2 of the License, or (at your 
 *  option) any later version. See the GNU General Public License for more 
 *  details at: http://www.embedthis.com/downloads/gplLicense.html
 *  
 *  This program is distributed WITHOUT ANY WARRANTY; without even the 
 *  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
 *  
 *  This GPL license does NOT permit incorporating this software into 
 *  proprietary programs. If you are unable to comply with the GPL, you must
 *  acquire a commercial license to use this software. Commercial licenses 
 *  for this software and support services are available from Embedthis 
 *  Software at http://www.embedthis.com 
 *  
 *  @end
 */
/************************************************************************/
/*
 *  End of file "../src/es/events/Timer.es"
 */
/************************************************************************/



/************************************************************************/
/*
 *  Start of file "../src/es/io/BinaryStream.es"
 */
/************************************************************************/

/*
 *  BinaryStream.es -- BinaryStream class. This class is a filter or endpoint stream to encode and decode binary types.
 *
 *  Copyright (c) All Rights Reserved. See details at the end of the file.
 */

module ejs.io {

    /**
     *  BinaryStreams encode and decode various objects onto streams. A BinaryStream may be stacked atop an underlying stream
     *  provider such as ByteArray, File or Http.
     *  @spec ejs
     *  @stability evolving
     */
    class BinaryStream implements Stream {

        use default namespace public

        /**
         *  Big endian byte order 
         */
        static const BigEndian: Number = ByteArray.BigEndian

        /**
         *  Little endian byte order 
         */
        static const LittleEndian: Number = ByteArray.LittleEndian

        /*
         *  Data input and output buffers. The buffers are used to marshall the data for encoding and decoding. The inbuf 
         *  also  hold excess input data. The outbuf is only used to encode data -- no buffering occurs.
         */
        private var inbuf: ByteArray
        private var outbuf: ByteArray
        private var nextStream: Stream

        /**
         *  Create a new BinaryStream
         *  @param stream Stream to stack upon.
         */
        function BinaryStream(stream: Stream) {
            if (!stream) {
                throw new ArgError("Must supply a Stream to connect with")
            }
            nextStream = stream
            inbuf = new ByteArray
            outbuf = new ByteArray

            /*
             *  Setup the input and output callbacks. These are invoked to get/put daa.
             */
            inbuf.input = function (buffer: ByteArray) {
                nextStream.read(buffer)
            }

            outbuf.output = function (buffer: ByteArray) {
                count = nextStream.write(buffer)
                buffer.readPosition += count
                buffer.reset()
            }
        }

        /**
         *  The number of bytes available to read
         */
        function get available(): Number
            inbuf.available

        /**
         *  Close the input stream and free up all associated resources.
         *  @param graceful if true, then close the socket gracefully after writing all pending data.
         */
        function close(graceful: Boolean = true): void {
            flush(graceful)
            nextStream.close(graceful)
        }

        /**
         *  Current byte ordering. Set to either LittleEndian or BigEndian
         */
        function get endian(): Number
            inbuf.endian

        /**
         *  Set the system encoding to little or big endian.
         *  @param value Set to true for little endian encoding or false for big endian.
         */
        function set endian(value: Number): Void {
            if (value != BigEndian && value != LittleEndian) {
                throw new ArgError("Bad endian value")
            }
            inbuf.endian = value
            outbuf.endian = value
        }

        /**
         *  Flush the stream and all stacked streams and underlying data source/sinks.
         *  @param graceful If true, then write all pending data.
         */
        function flush(graceful: Boolean = true): void {
            inbuf.flush(graceful)
            outbuf.flush(graceful)
            if (!(nextStream is ByteArray)) {
                nextStream.flush(graceful)
            }
        }

        /**
         *  Read data from the stream. 
         *  @param buffer Destination byte array for the read data.
         *  @param offset Offset in the byte array to place the data. If the offset is -1, then data is
         *      appended to the buffer write $position which is then updated. 
         *  @param count Number of bytes to read. If -1, read as much as the buffer will hold up.
         *  @returns a count of the bytes actually read. Returns zero on eof.
         *  @throws IOError if an I/O error occurs.
         */
        function read(buffer: ByteArray, offset: Number = 0, count: Number = -1): Number
            inbuf.read(buffer, offset, count)

        /**
         *  Read a boolean from the stream.
         *  @returns a boolean. Returns null on eof.
         *  @throws IOError if an I/O error occurs.
         */
        function readBoolean(): Boolean
            inbuf.readBoolean()

        /**
         *  Read a byte from the stream.
         *  @returns a byte. Returns -1 on eof.
         *  @throws IOError if an I/O error occurs.
         */
        function readByte(): Number
            inbuf.readByte()

        /**
         *  Read data from the stream into a byte array.
         *  @returns a new byte array with the available data. Returns an empty byte array on eof.
         *  @throws IOError if an I/O error occurs.
         *  @hide
         */
        # DEPRECATED
        function readByteArray(count: Number = -1): ByteArray
            inbuf.readByteArray(count)

        /**
         *  Read a date from the stream.
         *  @returns a date
         *  @throws IOError if an I/O error occurs.
         */
        function readDate(): Date
            inbuf.readDate()

        /**
         *  Read a double from the stream. The data will be decoded according to the encoding property.
         *  @returns a double
         *  @throws IOError if an I/O error occurs.
         */
        function readDouble(): Double
            inbuf.readDouble()

        /**
         *  Read a 32-bit integer from the stream. The data will be decoded according to the encoding property.
         *  @returns an 32-bitinteger
         *  @throws IOError if an I/O error occurs.
         */
        function readInteger(): Number
            inbuf.readInteger()

        /**
         *  Read a 64-bit long from the stream.The data will be decoded according to the encoding property.
         *  @returns a 64-bit long number
         *  @throws IOError if an I/O error occurs.
         */
        function readLong(): Number
            inbuf.readInteger()

        /**
         *  Read a UTF-8 string from the stream. 
         *  @param count of bytes to read. Returns the entire stream contents if count is -1.
         *  @returns a string
         *  @throws IOError if an I/O error occurs.
         */
        function readString(count: Number = -1): String 
            inbuf.readString(count)

        /**
         *  Read an XML document from the stream. This assumes the XML document will be the only data until EOF.
         *  @returns an XML document
         *  @throws IOError if an I/O error occurs.
         */
        function readXML(): XML {
            var data: String = ""
            while (1) {
                var s: String = inbuf.readString()
                if (s == null && data.length == 0) {
                    return null
                }
                if (s.length == 0) {
                    break
                }
                data += s
            }
            return new XML(data)
        }

        /**
         *  Write data to the stream. Write intelligently encodes various @data types onto the stream and will encode data 
         *  in a portable cross-platform manner according to the setting of the endian ByteStream property. If data is an 
         *  array, each element of the array will be written. The write call blocks until the underlying stream or endpoint 
         *  absorbes all the data. 
         *  @param data Data to write. The ByteStream class intelligently encodes various data types according to the
         *  current setting of the @endian BinaryStream property. 
         *  @returns The total number of elements that were written.
         *  @throws IOError if there is an I/O error.
         */
        function write(...data): Number {
            let count: Number = 0
            for each (i in data) {
                count += outbuf.write(i)
            }
            return count
        }

        /**
         *  Write a byte to the array. Data is written to the current write $position pointer.
         *  @param data Data to write
         */
        function writeByte(data: Number): Void 
            outbuf.writeByte(outbuf)

        /**
         *  Write a short to the array. Data is written to the current write $position pointer.
         *  @param data Data to write
         */
        function writeShort(data: Number): Void
            outbuf.writeShort(data)

        /**
         *  Write a double to the array. Data is written to the current write $position pointer.
         *  @param data Data to write
         */
        function writeDouble(data: Number): Void
            outbuf.writeDouble(data)

        /**
         *  Write a 32-bit integer to the array. Data is written to the current write $position pointer.
         *  @param data Data to write
         */
        function writeInteger(data: Number): Void {
            outbuf.writeInteger(data)
        }

        /**
         *  Write a 64 bit long integer to the array. Data is written to the current write $position pointer.
         *  @param data Data to write
         */
        function writeLong(data: Number): Void
            outbuf.writeLong(data)
    }
}


/*
 *  @copy   default
 *  
 *  Copyright (c) Embedthis Software LLC, 2003-2010. All Rights Reserved.
 *  Copyright (c) Michael O'Brien, 1993-2010. All Rights Reserved.
 *  
 *  This software is distributed under commercial and open source licenses.
 *  You may use the GPL open source license described below or you may acquire 
 *  a commercial license from Embedthis Software. You agree to be fully bound 
 *  by the terms of either license. Consult the LICENSE.TXT distributed with 
 *  this software for full details.
 *  
 *  This software is open source; you can redistribute it and/or modify it 
 *  under the terms of the GNU General Public License as published by the 
 *  Free Software Foundation; either version 2 of the License, or (at your 
 *  option) any later version. See the GNU General Public License for more 
 *  details at: http://www.embedthis.com/downloads/gplLicense.html
 *  
 *  This program is distributed WITHOUT ANY WARRANTY; without even the 
 *  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
 *  
 *  This GPL license does NOT permit incorporating this software into 
 *  proprietary programs. If you are unable to comply with the GPL, you must
 *  acquire a commercial license to use this software. Commercial licenses 
 *  for this software and support services are available from Embedthis 
 *  Software at http://www.embedthis.com 
 *  
 *  @end
 */
/************************************************************************/
/*
 *  End of file "../src/es/io/BinaryStream.es"
 */
/************************************************************************/



/************************************************************************/
/*
 *  Start of file "../src/es/io/File.es"
 */
/************************************************************************/

/*
 *  File.es -- File I/O class. Do file I/O and manage files.
 *
 *  Copyright (c) All Rights Reserved. See details at the end of the file.
 */

module ejs.io {

    /**
     *  The File class provides a foundation of I/O services to interact with physical files.
     *  Each File object represents a single file, a named path to data stored in non-volatile memory. A File object 
     *  provides methods for creating, opening, reading, writing and deleting a file, and for accessing and modifying 
     *  information about the file. All I/O is unbuffered and synchronous.
     *  @spec ejs
     *  @stability evolving
     */
    native class File implements Stream {

        use default namespace public

        /**
         *  Create a File object and open the requested path.
         *  @param path the name of the file to associate with this file object. Can be either a String or a Path.
         *  @param options If the options are provided, the file is opened. See $open for the available options.
         */
        native function File(path: Object, options: Object = null)

        /**
         *  Is the file opened for reading
         */
        native function get canRead(): Boolean

        /**
         *  Is the file opened for writing.
         */
        native function get canWrite(): Boolean

        /**
         *  Close the input stream and free up all associated resources.
         *  @param graceful if true, then close the file gracefully after writing all pending data.
         */
        native function close(graceful: Boolean = true): void 

        /**
         *  Flush any buffered data. This is a noop and is supplied just to comply with the Stream interface.
         *  @param graceful If true, write all pending data.
         */  
        native function flush(graceful: Boolean = true): void

        /**
         *  Iterate over the positions in a file. This will get an iterator for this file to be used by 
         *      "for (v in files)"
         *  @return An iterator object that will return numeric offset positions in the file.
         */
        override iterator native function get(): Iterator

        /**
         *  Get an iterator for this file to be used by "for each (v in obj)". Return each byte of the file in turn.
         *  @return An iterator object that will return the bytes of the file.
         */
        override iterator native function getValues(): Iterator

        /**
         *  Is the file open
         */
        native function get isOpen(): Boolean

        /** 
         *  Open a file. This opens the file designated when the File constructor was called.
         *  @params options Optional options. If ommitted, the options default to open the file in read mode.
         *      Options can be either a mode string or can be an options hash. 
         *  @options mode optional file access mode string. Use "r" for read, "w" for write, "a" for append to existing
         *      content, "+" never truncate. Defaults to "r".
         *  @options permissions Number containing the Posix permissions value. Note: this is a number and not a string
         *      representation of an octal posix number.
         *  @return the File object. This permits method chaining.
         *  @throws IOError if the path or file cannot be created.
         */
        native function open(options: Object = null): File

        /**
         *  Current file options set when opening the file.
         */ 
        native function get options(): Object

        /**
         *  The name of the file associated with this File object or null if there is no associated file.
         */
        native function get path(): Path 

        /**
         *  The current read/write I/O position in the file.
         */
        native function get position(): Number

        /**
         *  Seek to a new location in the file and set the File marker to a new read/write position.
         *  @param loc Location in the file to seek to. Set the position to zero to reset the position to the beginning 
         *  of the file. Set the position to a negative number to seek relative to the end of the file (-1 positions 
         *  at the end of file).
         *  @throws IOError if the seek failed.
         */
        native function set position(loc: Number): void

        /**
         *  Read a block of data from a file into a byte array. This will advance the read file's position.
         *  @param buffer Destination byte array for the read data.
         *  @param offset Offset in the byte array to place the data. If the offset is -1, then data is
         *      appended to the buffer write $position which is then updated. 
         *  @param count Number of bytes to read. If -1, read much as the buffer will hold up to the entire file if the 
         *  buffer is of sufficient size or is growable.
         *  @return A count of the bytes actually read. Returns 0 on end of file.
         *  @throws IOError if the file could not be read.
         */
        native function read(buffer: ByteArray, offset: Number = 0, count: Number = -1): Number

        /**
         *  Read data bytes from a file and return a byte array containing the data.
         *  @param count Number of bytes to read. If null, read the entire file.
         *  @return A byte array containing the read data. Returns an empty array on end of file.
         *  @throws IOError if the file could not be read.
         */
        native function readBytes(count: Number = -1): ByteArray

        /**
         *  Read data from a file as a string.
         *  @param count Number of bytes to read. If -1, read the entire file.
         *  @return A string containing the read data. Returns an empty string on end of file.
         *  @throws IOError if the file could not be read.
         */
        native function readString(count: Number = -1): String

        /**
         *  Remove a file
         *  @throws IOError if the file could not be removed.
         */
        function remove(): Void {
            if (isOpen) {
                throw new IOError("File is open")
            }
            Path(path).remove()
        }

        /**
         *  The size of the file in bytes.
         */
        native function get size(): Number 

        /**
         *  Truncate the file. 
         *  @param value the new length of the file
         *  @throws IOError if the file's size cannot be changed
         */
        native function truncate(value: Number): Void 

        /**
         *  Write data to the file. If the stream is in sync mode, the write call blocks until the underlying stream or 
         *  endpoint absorbes all the data. If in async-mode, the call accepts whatever data can be accepted immediately 
         *  and returns a count of the elements that have been written.
         *  @param items The data argument can be ByteArrays, strings or Numbers. All other types will call serialize
         *  first before writing. Note that numbers will not be written in a cross platform manner. If that is required, use
         *  the BinaryStream class to control the byte ordering when writing numbers.
         *  @returns the number of bytes written.  
         *  @throws IOError if the file could not be written.
         */
        native function write(...items): Number
    }
}

/*
 *  @copy   default
 *  
 *  Copyright (c) Embedthis Software LLC, 2003-2010. All Rights Reserved.
 *  Copyright (c) Michael O'Brien, 1993-2010. All Rights Reserved.
 *  
 *  This software is distributed under commercial and open source licenses.
 *  You may use the GPL open source license described below or you may acquire 
 *  a commercial license from Embedthis Software. You agree to be fully bound 
 *  by the terms of either license. Consult the LICENSE.TXT distributed with 
 *  this software for full details.
 *  
 *  This software is open source; you can redistribute it and/or modify it 
 *  under the terms of the GNU General Public License as published by the 
 *  Free Software Foundation; either version 2 of the License, or (at your 
 *  option) any later version. See the GNU General Public License for more 
 *  details at: http://www.embedthis.com/downloads/gplLicense.html
 *  
 *  This program is distributed WITHOUT ANY WARRANTY; without even the 
 *  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
 *  
 *  This GPL license does NOT permit incorporating this software into 
 *  proprietary programs. If you are unable to comply with the GPL, you must
 *  acquire a commercial license to use this software. Commercial licenses 
 *  for this software and support services are available from Embedthis 
 *  Software at http://www.embedthis.com 
 *  
 *  @end
 */
/************************************************************************/
/*
 *  End of file "../src/es/io/File.es"
 */
/************************************************************************/



/************************************************************************/
/*
 *  Start of file "../src/es/io/FileSystem.es"
 */
/************************************************************************/

/*
 *  FileSystem.es -- FileSystem class
 *
 *  Copyright (c) All Rights Reserved. See details at the end of the file.
 */

module ejs.io {

    /**
     *  The FileSystem class provides access to information about file systems hosting files.
     *  @spec ejs
     *  @stability prototype
     */
    native class FileSystem {

        use default namespace public

        /**
         *  Create a new FileSystem object based on the given path.
         *  @param path String or Path of the file system
         */
        native function FileSystem(path: Object)

        /**
         *  Do path names on this file system support drive specifications.
         */
        native function get hasDrives(): Boolean 

        /**
         *  The new line characters for this file system. Usually "\n" or "\r\n".
         */
        native function get newline(): String 

        /**
         */
        native function set newline(terminator: String): Void

        /**
         *  Path to the root directory of the file system
         */
        native function get root(): Path

        /**
            Path directory separators. The first character is the default separator. Usually "/" or "\\".
         */
        native function get separators(): String 

        /**
         */
        native function set separators(sep: String): Void 
    }
}

/*
 *  @copy   default
 *  
 *  Copyright (c) Embedthis Software LLC, 2003-2010. All Rights Reserved.
 *  Copyright (c) Michael O'Brien, 1993-2010. All Rights Reserved.
 *  
 *  This software is distributed under commercial and open source licenses.
 *  You may use the GPL open source license described below or you may acquire 
 *  a commercial license from Embedthis Software. You agree to be fully bound 
 *  by the terms of either license. Consult the LICENSE.TXT distributed with 
 *  this software for full details.
 *  
 *  This software is open source; you can redistribute it and/or modify it 
 *  under the terms of the GNU General Public License as published by the 
 *  Free Software Foundation; either version 2 of the License, or (at your 
 *  option) any later version. See the GNU General Public License for more 
 *  details at: http://www.embedthis.com/downloads/gplLicense.html
 *  
 *  This program is distributed WITHOUT ANY WARRANTY; without even the 
 *  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
 *  
 *  This GPL license does NOT permit incorporating this software into 
 *  proprietary programs. If you are unable to comply with the GPL, you must
 *  acquire a commercial license to use this software. Commercial licenses 
 *  for this software and support services are available from Embedthis 
 *  Software at http://www.embedthis.com 
 *  
 *  @end
 */
/************************************************************************/
/*
 *  End of file "../src/es/io/FileSystem.es"
 */
/************************************************************************/



/************************************************************************/
/*
 *  Start of file "../src/es/io/Http.es"
 */
/************************************************************************/

/**
    Http.es -- HTTP client side communications
 *
    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

module ejs.io {

    /**
        The Http object represents a Hypertext Transfer Protocol version 1.1 client connection. It is used to issue 
        HTTP requests and capture responses. It supports the HTTP/1.1 standard including methods for GET, POST, 
        PUT, DELETE, OPTIONS, and TRACE. It also supports Keep-Alive and SSL connections. 
        @spec ejs
        @stability prototype
     */
    native class Http implements Stream {

        use default namespace public

        /** 
          HTTP Continue Status (100)
         */     
        static const Continue : Number   = 100

        /** 
            HTTP Success Status (200) 
         */     
        static const Ok : Number   = 200

        /** 
            HTTP Created Status (201) 
         */     
        static const Created : Number   = 201

        /** 
            HTTP Accepted Status (202) 
         */     
        static const Accepted : Number   = 202

        /** 
            HTTP Non-Authoritative Information Status (203) 
         */     
        static const NotAuthoritative : Number   = 203

        /** 
            HTTP No Content Status (204)  
         */     
        static const NoContent : Number   = 204

        /** 
            HTTP Reset Content Status (205) 
         */     
        static const Reset : Number   = 205

        /** 
            HTTP Partial Content Status (206) 
         */     
        static const PartialContent : Number   = 206

        /** 
            HTTP Multiple Choices Status (300) 
         */     
        static const MultipleChoice : Number   = 300

        /** 
            HTTP Moved Permanently Status (301) 
         */     
        static const MovedPermanently : Number   = 301

        /** 
            HTTP Found but Moved Temporily Status (302) 
         */     
        static const MovedTemporarily : Number   = 302

        /** 
            HTTP See Other Status (303) 
         */     
        static const SeeOther : Number   = 303

        /** 
            HTTP Not Modified Status (304)     
         */
        static const NotModified : Number   = 304

        /** 
            HTTP Use Proxy Status (305) 
         */     
        static const UseProxy : Number   = 305

        /** 
            HTTP Bad Request Status(400) 
         */     
        static const BadRequest : Number   = 400

        /** 
            HTTP Unauthorized Status (401) 
         */     
        static const Unauthorized : Number   = 401

        /** 
            HTTP Payment Required Status (402) 
         */     
        static const PaymentRequired : Number   = 402

        /** 
            HTTP Forbidden Status (403)  
         */     
        static const Forbidden : Number   = 403

        /** 
            HTTP Not Found Status (404) 
         */     
        static const NotFound : Number   = 404

        /** 
            HTTP Method Not Allowed Status (405) 
         */     
        static const BadMethod : Number   = 405

        /** 
            HTTP Not Acceptable Status (406) 
         */     
        static const NotAcceptable : Number   = 406

        /** 
            HTTP ProxyAuthentication Required Status (407) 
         */     
        static const ProxyAuthRequired : Number   = 407

        /** 
            HTTP Request Timeout Status (408) 
         */     
        static const RequestTimeout : Number   = 408

        /** 
            HTTP Conflict Status (409) 
         */     
        static const Conflict : Number   = 409

        /** 
            HTTP Gone Status (410) 
         */     
        static const Gone : Number   = 410

        /** 
            HTTP Length Required Status (411) 
         */     
        static const LengthRequired : Number   = 411
        
        /** 
            HTTP Precondition Failed Status (412) 
         */     
        static const PrecondFailed : Number   = 412

        /** 
            HTTP Request Entity Too Large Status (413) 
         */     
        static const EntityTooLarge : Number   = 413

        /** 
            HTTP Request URI Too Long Status (414)  
         */     
        static const UriTooLong : Number   = 414

        /** 
            HTTP Unsupported Media Type (415) 
         */     
        static const UnsupportedMedia : Number   = 415

        /** 
            HTTP Requested Range Not Satisfiable (416) 
         */     
        static const BadRange : Number   = 416

        /** 
            HTTP Server Error Status (500) 
         */     
        static const ServerError : Number   = 500

        /** 
            HTTP Not Implemented Status (501) 
         */     
        static const NotImplemented : Number   = 501

        /** 
            HTTP Bad Gateway Status (502) 
         */     
        static const BadGateway : Number   = 502

        /** 
            HTTP Service Unavailable Status (503) 
         */     
        static const ServiceUnavailable : Number   = 503

        /** 
            HTTP Gateway Timeout Status (504) 
         */     
        static const GatewayTimeout : Number   = 504

        /** 
            HTTP Http Version Not Supported Status (505) 
         */     
        static const VersionNotSupported: Number   = 505

        /**
            Callback event mask for readable events
         */
        static const Read: Number = 2

        /**
            Callback event mask for writeable events
         */
        static const Write: Number = 4

        private var _response: String

        /**
            Create an Http object. The object is initialized with the URI
            @param uri The (optional) URI to initialize with.
            @throws IOError if the URI is malformed.
         */
        native function Http(uri: String = null)

        /**
            Add a request header. Must be done before the Http request is issued. 
            @param key The header keyword for the request, e.g. "accept".
            @param value The value to associate with the header, e.g. "yes"
            @param overwrite If true, overwrite existing headers of the same key name.
         */
        native function addHeader(key: String, value: String, overwrite: Boolean = true): Void

        /**
            The number of response data bytes that are currently available for reading.
            This API is not supported and is provided only for compliance with the Stream interface.
            @hide
         */
        native function get available(): Number 

        /**
            Return whether chunked transfer encoding will be applied to the body data of the request. Chunked encoding
            is used when an explicit request content length is unknown at the time the request headers must be emitted.
            Chunked encoding is automatically enabled if $post, $put or $upload is called and a contentLength has not been 
            previously set. This call is deprecated.
            @hide
         */
        native function get chunked(): Boolean

        /**
            Control whether chunked transfer encoding will be applied to the body data of the request. Chunked encoding
            is used when an explicit request content length is unknown at the time the request headers must be emitted.
            Chunked encoding is automatically enabled if $post, $put or $upload is called and a contentLength has not been 
            previously set. This call is deprecated.
            @param value Boolean value. Set to true to enable chunked transfers.
            @hide
         */
        native function set chunked(value: Boolean): Void

        /**
            Close the connection to the server and free up all associated resources. 
            This closes any open network connection and resets the http object to be ready for another connection. 
            Connections should be explicitly closed rather than relying on the garbage collector to dispose of the 
            Http object and automatically close the connection.
            @param graceful if true, then close the socket gracefully after writing all pending data.
            @stability prototype
         */
        native function close(graceful: Boolean = true): Void 

        /**
            Issue a HTTP request for the current method and uri. The HTTP method should be defined via the $method 
            property and uri via the $uri property. This routine is typically not used. Rather $get, $head, $post or $put
            are used instead.
            @param uri New uri to use. This overrides any previously defined uri for the Http object.
            @param data Data objects to send with the request. Data is written raw and is not encoded or converted. 
                However, the routine intelligently handles arrays such that, each element of the array will be written. 
            @throws IOError if the request cannot be issued to the remote server.
         */
        native function connect(uri: String = null, ...data): Void

        /**
            Filename of the certificate file for this HTTP object. The certificate is only used if secure communications 
            are in use.
            @hide
         */
        native function get certificate(): Path

        /**
            @duplicate Http.certificate
            @param certFile The name of the certificate file.
            @throws IOError if the file does not exist.
         */
        native function set certificate(certFile: Path): Void

        /**
            Http response status code.  Set to an integer value or -1 if the Http response code is not known. e.g. 200.  
         */
        native function get code(): Number

        /**
            Http response message from the Http response status line, e.g. OK. This is the additional text added to the
            first line in the Http protocol response.
         */
        native function get codeString(): String

        /**
            Content encoding used for the response. Set to the value provided by the Http Content-Type response header or
            or null if not known.
            @hide
         */
        native function get contentEncoding(): String

        /**
            Request/Response body length. Reading this property, will return the response content body length. The value
            will be the response body length in bytes or -1 if no body or if the body length is not known.
            Setting this value will set the outgoing request body length. This will set the request Content-Length 
            header value and is used when using $write to manually send the requeset body data.
            WARNING: This API will change in the next release. It will become a sole getter. Use addHeader to define
            a Content-Length header if you require defining a request body content length.
            @stability prototype
         */
        native function get contentLength(): Number

        /**
            @duplicate Http.contentLength
            @param length The length of body data that will be sent with the request.
         */
        native function set contentLength(length: Number): Void

        /**  DEPRECATED @hide */
        function get bodyLength(): Void
            contentLength

        /**  DEPRECATED @hide */
        function set bodyLength(value: Number): Void {
            contentLength = value
        }

        /**
            Response content type derrived from the response Http Content-Type header.
         */
        native function get contentType(): String

        /**
            When the response was generated. Response date derrived from the response Http Date header.
         */
        native function get date(): Date

        /**
            Issue a DELETE request for the current uri. This function will block until the the request completes or 
            fails, unless a callback function has been defined. If a callback has been specified, this function 
            will initiate the request and return immediately.
            @param uri The uri to delete. This overrides any previously defined uri for the Http object.
            @param data Data objects to send with the request. Data is written raw and is not encoded or converted. 
                However, the routine intelligently handles arrays such that, each element of the array will be written. 
            @throws IOError if the request cannot be issued to the remote server.
         */
        native function del(uri: String = null, ...data): Void

        /**
            When the response content expires. This is derrived from the response Http Expires header.
         */
        native function get expires(): Date

        /**
            Flush any buffered write data
            @param graceful If true, then write all pending.
         */
        native function flush(graceful: Boolean = true): Void

        /**
            Will response redirects be automatically followed by this Http object. If true, the Http class will 
            automatically reissue requests if redirected by the server.
         */
        native function get followRedirects(): Boolean

        /**
            Eanble or disable following redirects from the connection remove server. Default is true.
            @param flag Set to true to follow redirects.
         */
        native function set followRedirects(flag: Boolean): Void

        /**
            Issue a POST request with form data the current uri. This function will block until the the request 
            completes or fails, unless a callback function has been defined. If a callback has been specified, 
            this function will initiate the request and return immediately.
            @param uri Optional request uri. If non-null, this overrides any previously defined uri for the Http object.
            @param postData Optional object hash of key value pairs to use as the post data. These are www-url-encoded and
                the content mime type is set to "application/x-www-form-urlencoded".
            @throws IOError if the request cannot be issued to the remote server.
         */
        native function form(uri: String, postData: Object): Void

        /**
            Issue a GET request for the current uri. This call initiates a GET request. It does not wait for the
            request to complete. Once initiated, one of the $read or response routines  may be used to receive the 
            response data.
            @param uri The uri to get. This overrides any previously defined uri for the Http object.
            @param data Data objects to send with the request. Data is written raw and is not encoded or converted. 
                However, the routine intelligently handles arrays such that, each element of the array will be written. 
            @throws IOError if the request cannot be issued to the remote server.
         */
        native function get(uri: String = null, ...data): Void

        /**
            Issue a HEAD request for the current uri. This call initiates a HEAD request. Once initiated, $read and $write
            may be issued to send and receive data.
            @param uri The request uri. This overrides any previously defined uri for the Http object.
            @throws IOError if the request cannot be issued to the remote server.
         */
        native function head(uri: String = null): Void

        /**
            Get the value of any response header field.
            @return The header field value as a string or null if not known.
         */
        native function header(key: String): String

        /**
            Response headers. Use header() to retrieve a single header value.
         */
        native function get headers(): Array

        /**
            Is the connection is utilizing SSL.
         */
        native function get isSecure(): Boolean

        /**
            Private key file for this Https object.
            NOT currently supported.
            @return The file name.
            @hide
         */
        native function get key(): Path

        /**
            @duplicate Http.key
            @param keyFile The name of the key file.
            @throws IOError if the file does not exist.
            @hide
         */
        native function set key(keyFile: Path): Void

        /**
            When the response content was last modified. Set to the the value of the response Http Last-Modified header.
            Set to null if not known.
         */
        native function get lastModified(): Date

        /**
            Http request method for this Http object.
         */
        native function get method(): String

        /**
            Set or reset the Http object's request method. Default method is GET.
            @param name The method name as a string.
            @throws IOError if the request is not GET, POST, HEAD, OPTIONS, PUT, DELETE or TRACE.
         */
        native function set method(name: String)

        /**
            Get the mime type for a path or extension string.
            NOTE: this routine will migrate to the Url class in the future.
            @param path Path or extension string to examine.
            @returns The mime type string
         */
        native static function mimeType(path: String): String

        /**
            Issue an OPTIONS request for the current uri. Use $readString to retrieve the response.
            @param uri New uri to use. This overrides any previously defined uri for the Http object.
            @throws IOError if the request cannot be issued to the remote server.
         */
        native function options(uri: String = null): Void

        /**
            Initiate a POST request for the current uri. Posted data is NOT URL encoded. If you want to post data to a 
            form, consider using the $form method instead which automatically URL encodes the data. Post data may be 
            supplied may alternatively be supplied via $write. If a contentLength has not been previously defined for this
            request, chunked transfer encoding will be enabled (See $chunked). 
            @param uri Optional request uri. If non-null, this overrides any previously defined uri for the Http object.
            @param data Data objects to send with the post request. Data is written raw and is not encoded or converted. 
                However, this routine intelligently handles arrays such that, each element of the array will be written. 
            @throws IOError if the request cannot be issued to the remote server.
         */
        native function post(uri: String, ...data): Void

        /**
            Issue a PUT request for the current uri. This call initiates a PUT request. If a contentLength has not been 
            previously defined for this request, chunked transfer encoding will be enabled (See $chunked). 
            @param uri The uri to delete. This overrides any previously defined uri for the Http object.
            @param data Optional data objects to write to the request stream. Data is written raw and is not encoded 
                or converted.  However, put intelligently handles arrays such that, each element of the array will be 
                written. If encoding of put data is required, use the BinaryStream filter. If no putData is supplied,
                and the $contentLength is non-zero you must call $write to supply the body data.
            @param data Optional object hash of key value pairs to use as the post data.
            @throws IOError if the request cannot be issued to the remote server.
         */
        native function put(uri: String, ...data): Void

        /**
            Read a block of response content data from the connection.
            @param buffer Destination byte array for the read data.
            @param offset Offset in the byte array to place the data. If offset is -1, data is appended at the 
                current byte array write position.
            @param count Number of bytes to read. 
            @returns a count of the bytes actually read. This call may return with fewer bytes read than requested.
                If a callback has been defined, this call will not block and may return zero if no data is currently 
                available. If no callback has been defined, it will block.
            @throws IOError if an I/O error occurs.
         */
        native function read(buffer: ByteArray, offset: Number = 0, count: Number = -1): Number

        /**
            Read the request response as a string.
            @param count of bytes to read. Returns the entire response contents if count is -1.
            @returns a string of $count characters beginning at the start of the response data.
            @throws IOError if an I/O error occurs.
         */
        native function readString(count: Number = -1): String

        /**
            Read the request response as an array of lines.
            @param count of linese to read. Returns the entire response contents if count is -1.
            @returns an array of strings
            @throws IOError if an I/O error occurs.
         */
        native function readLines(count: Number = -1): Array

        /**
            Read the request response as an XML document.
            @returns the response content as an XML object 
            @throws IOError if an I/O error occurs.
         */
        function readXml(): XML
            XML(response)

        /**
            Response body content. The first time this property is read, the response content will be read and buffered.
            Set to the response as a string of characters. If the response has no body content, the empty string will 
            be returned.
            @throws IOError if an I/O error occurs.
         */
        native function get response(): String

        /**
            The maximum number of retries for a request. Retries are essential as the HTTP protocol permits a 
            server or network to be unreliable. The default retries is 2.
            @hide
         */
        native function get retries(): Number

        /**
            Define the number of retries of a request. Retries are essential as the HTTP protocol permits a server or
            network to be unreliable. The default retries is 2.
            @param count Number of retries. A retry count of 1 will retry a failed request once.
            @hide
         */
        native function set retries(count: Number): Void

        /**
            WARNING: this feature will be replaced with a listener based interface in the next release.
            Define a callback to be invoked for readable and/or writable events. Callbacks are used when writing event
            based programs that must not block. When a callback is defined, the $read and $write routines will not block. 
            @param eventMask Mask of events of interest. Select from Read, Write.
            @param cb Callback function to invoke in response to I/O events.
            <pre>
                function callback(e: Event): Void
            </pre>
            Where e.data == http. The event arg may be either a HttpDataEvent or a HtttpErrorEvent. The callback is
            invoked when there is an error, response data to read  or on end of request where $available == 0 and a read
                will return null). It may also invoked to signal that the underlying socket can accept write data. 
            The HttpError event will be passed  on any request processing errors. Does not include remote server errors.
            @hide
         */
        native function setCallback(eventMask: Number, cb: Function): Void

        /**
            Set the user credentials to use if the request requires authentication.
         */
        native function setCredentials(username: String, password: String): Void

        /**
            Request timeout in milliseconds. This is the idle timeout value. If the request has no I/O activity for 
            this time period, it will be retried or aborted.
         */
        native function get timeout(): Number

        /**
            Set the request timeout.
            @param timeout Number of milliseconds to block while attempting requests. -1 means no timeout.
         */
        native function set timeout(timeout: Number): Void

        /**
            Issue a TRACE request for the current uri. Use $readString to retrieve the response.
            @param uri New uri to use. This overrides any previously defined uri for the Http object.
            @throws IOError if the request cannot be issued to the remote server.
         */
        native function trace(uri: String = null): Void

        /**
            Upload files using multipart/mime. This routine initiates a POST request and sends the specified files
            and form fields using multipart mime encoding. This call also automatically enables chunked transfer 
            encoding (See $chunked). It can only be used on servers that accept HTTP/1.1 request which should be all 
            servers.
            @param url The url to upload to. This overrides any previously defined uri for the Http object.
            @param files Object hash of files to upload
            @param fields Object hash of of form fields to send
            @example
                fields = { name: "John Smith", address: "700 Park Avenue" }
                files = { file1: "a.txt, file2: "b.txt" }
                http.upload(URL, files, fields)
         */
        function upload(url: String, files: Object, fields: Object = null): Void {
            let boundary = "<<Upload Boundary>>"
            let buf = new ByteArray(4096)
            let http = this
            buf.output = function (buf) {
                http.write(buf)
                buf.flush(false)
            }
            addHeader("Content-Type", "multipart/form-data; boundary=" + boundary)
            post(url)
            if (fields) {
                for (key in fields) {
                    buf.write('--' + boundary + "\r\n")
                    buf.write('Content-Disposition: form-data; name=' + encodeURI(key) + "\r\n")
                    buf.write('Content-Type: application/x-www-form-urlencoded\r\n\r\n')
                    buf.write(encodeURI(fields[key]) + "\r\n")
                }
            }
            for (key in files) {
                file = files[key]
                buf.write('--' + boundary + "\r\n")
                buf.write('Content-Disposition: form-data; name=' + key + '; filename=' + Path(file).basename + "\r\n")
                buf.write('Content-Type: ' + mimeType(file) + "\r\n\r\n")

                f = File(file).open()
                data = new ByteArray
                while (f.read(data)) {
                    buf.write(data)
                }
                f.close()
                buf.write("\r\n")
            }
            buf.write('--' + boundary + "--\r\n\r\n")
            buf.flush()
            http.wait()
        }

        /**
            URI for this Http object.
         */
        native function get uri(): String

        /**
            Set or reset the Http object's URI.
            @param newUri The URI as a string.
            @throws IOError if the URI is malformed.
         */
        native function set uri(newUri: String): Void

        /**
            Wait for a request to complete.
            @param timeout Time in seconds to wait for the request to complete
            @return True if the request successfully completes.
         */
        native function wait(timeout: Number = -1): Boolean

        /**
            Write body data to the request stream. The write call blocks while writing data. The Http.contentLength should 
            normally be set prior to writing any data to ensure that the request "Content-length" header is properly 
            defined. If the body length has not been defined, the data will be transferred using chunked transfers. In
            this case, you should call write() with no data to signify the end of the write stream.
            the Content-Length header will not be defined and the remote server will have to close the underling 
            HTTP connection at the completion of the request. This will prevent HTTP keep-alive for subsequent 
            requests.
            @param data Data objects to write to the request stream. Data is written raw and is not encoded or converted. 
                However, write intelligently handles arrays such that, each element of the array will be written. 
                If encoding of write data is required, use the BinaryStream filter. 
            @throws StateError if the Http method is not set to POST.
            @throws IOError if more post data is written than specified via the contentLength property.
         */
        native function write(...data): Void
    }

    /**
        WARNING: this feature will be replaced with a listener based interface in the next release.
        Data event issued to the callback function.
        @hide
     */
    class HttpDataEvent extends Event {
        /**
            Mask of pending events. Set to include $Read and/or $Write values.
         */
        public var eventMask: Number
    }

    /**
        WARNING: this feature will be replaced with a listener based interface in the next release.
        Error event issued to the callback function if any errors occur during an Http request.
        @hide
     */
    class HttpErrorEvent extends Event {
    }
}


/*
    @copy   default
    
    Copyright (c) Embedthis Software LLC, 2003-2010. All Rights Reserved.
    Copyright (c) Michael O'Brien, 1993-2010. All Rights Reserved.
    
    This software is distributed under commercial and open source licenses.
    You may use the GPL open source license described below or you may acquire 
    a commercial license from Embedthis Software. You agree to be fully bound 
    by the terms of either license. Consult the LICENSE.TXT distributed with 
    this software for full details.
    
    This software is open source; you can redistribute it and/or modify it 
    under the terms of the GNU General Public License as published by the 
    Free Software Foundation; either version 2 of the License, or (at your 
    option) any later version. See the GNU General Public License for more 
    details at: http://www.embedthis.com/downloads/gplLicense.html
    
    This program is distributed WITHOUT ANY WARRANTY; without even the 
    implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
    
    This GPL license does NOT permit incorporating this software into 
    proprietary programs. If you are unable to comply with the GPL, you must
    acquire a commercial license to use this software. Commercial licenses 
    for this software and support services are available from Embedthis 
    Software at http://www.embedthis.com 
    
    @end
 */
/************************************************************************/
/*
 *  End of file "../src/es/io/Http.es"
 */
/************************************************************************/



/************************************************************************/
/*
 *  Start of file "../src/es/io/Path.es"
 */
/************************************************************************/

/*
    Path.es --  Path class. Path's represent files in a file system. The file may or may not exist.  

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

module ejs.io {

    /**
        Path class. Paths are filenames and may or may not represent physical files in a file system. That is, the 
        corresponding file or directory for the Path  may or may not exist. Once created, Paths are immutable and their
        path value cannot be changed.
        @spec ejs
        @stability evolving
     */
    native class Path {

        use default namespace public

        /**
            Create a new Path object and set the path's location.
            @param path Name of the path to associate with this path object.
         */
        native function Path(path: String)

        /**
            An absolute path equivalent for the current path. The path is normalized.
         */
        native function get absolute(): Path

        /**
            Get when the file was last accessed. Set to null if the file does not exist.
         */
        native function get accessed(): Date 

        /**
            Base of portion of the path. The base portion is the trailing portion without any directory elements.
         */
        native function get basename(): Path
        
        /**
            Path components. This is the path converted to an absolute path and then broken into components for each
            directory level. It is set to an array object with an element for each segment of the path. The first 
                element represents the file system root and will be the empty string or drive spec on a windows 
                like systems. Array.join("/") can be used to put components back into a complete path.
         */
        native function get components(): Array
  
        /**
            Test if the path name contains a substring
            @param pattern String pattern to search for
            @return Boolean True if found.
         */
        function contains(pattern: String): Boolean
            name.contains(pattern)

        /**
            Copy a file
            @param target New file location
            @param options Object hash
            @options permissions Set to a numeric Posix permissions mask. Not implemented.
         */
        native function copy(target: Object, options: Object = null): Void

        /**
            When was the file was created. Set to null if the file does not exist.
         */
        native function get created(): Date 

        /**
            The directory portion of a file. The directory portion is the leading portion including all 
            directory elements and excluding the base name. On some systems, it will include a drive specifier.
         */
        native function get dirname(): Path

        /**
            Return true if the path ends with the given suffix
            @param suffix String suffix to compare with the path.
            @return true if the path does begin with the suffix
         */
        function endsWith(suffix: String): Boolean
            this.toString().endsWith(suffix)

        /**
            Does a file exist for this path.
         */
        native function get exists(): Boolean 

        /**
            File extension portion of the path. The file extension is the portion after (and not including) the last ".".
         */
        native function get extension(): String 

        /**
            Find matching files. Files are listed in depth first order.
            @param glob Glob style Pattern that files must match. This is similar to a ls() style pattern.
            @param recurse Set to true to examine sub-directories. 
            @return Return a list of matching files
         */
        function find(glob: String = null, recurse: Boolean = true): Array {
            function recursiveFind(path: Path, pattern: RegExp): Array {
                let result: Array = []
                if (path.isDir) {
                    for each (f in Path(path).files(true)) {
                        let got: Array = recursiveFind(f, pattern)
                        for each (i in got) {
                            result.append(i)
                        }
                    }
                }
                if (path.basename.toString().match(pattern)) {
                    result.append(path)
                }
                return result
            }
            if (glob == null) {
                glob = "*"
            }
            pattern = RegExp("^" + glob.replace(/\./g, "\\.").replace(/\*/g, ".*") + "$")
            return recursiveFind(this, pattern)
        }

        /**
            Get a list of files in a directory. The returned array contains the base path portion only, relative to the
            path itself. This routine does not recurse into sub-directories. To get a list of files in sub-directories,
            use find().
            @param enumDirs If set to true, then dirList will include sub-directories in the returned list of files.
            @return An Array of Path objects for each file in the directory.
         */
        native function files(enumDirs: Boolean = false): Array 

        /**
            Get iterate over any files contained under this path (assuming it is a directory) "for (v in files)". 
                This operates the same as getValues on a Path object.
            @return An iterator object.
            @example:
                for (f in Path("."))
         */
        override iterator native function get(): Iterator

        /**
            Get an iterator for this file to be used by "for each (v in obj)". Return 
                This operates the same as $get on a Path object.
            @return An iterator object.
            @example
                for each (f in Path("."))
         */
        override iterator native function getValues(): Iterator

        /**
            Does the file path have a drive spec (C:) in it's name. Only relevant on Windows like systems.
         */
        native function get hasDrive(): Boolean 

        /**
            Is the path absolute, i.e. Beginning at the file system's root.
         */
        native function get isAbsolute(): Boolean

        /**
            Is the path a directory
         */
        native function get isDir(): Boolean

        /**
            Is the path is a symbolic link. Not available on some platforms (such as Windows and VxWorks).
         */
        native function get isLink(): Boolean

        /**
            Is the path a regular file
         */
        native function get isRegular(): Boolean

        /**
            Is the path is relative and not absolute.
         */
        native function get isRelative(): Boolean

        /**
            Join paths. Joins one more more paths together. If the other path is absolute, return it. Otherwise
            add a separator, and continue joining. The separator is chosen to match the first separator found in 
            either path. If none found, the default file system path separator is used.
            @return A new joined, normalized Path object.
         */
        native function join(...other): Path

        /**
            Join an extension to a path. If the base name of the path already has an extension, this call does nothing.
            @return A path with extension.
         */
        native function joinExt(ext: String): Path

        /**
            The length of the path name in bytes. Note: this is not the file size. For that, use Path.size
         */
        override native function get length(): Number 

        /**
            The target pointed to if this path is a symbolic link. Not available on some platforms such as Windows and 
            VxWorks. If the path is not a symbolic link, it is set to null.
         */
        native function get linkTarget(): Path

        /**
            Make a new directory and all required intervening directories. If the directory already exists, 
                the function returns without throwing an exception.
            @param options
            @options permissions Set to a numeric Posix permissions mask
            @options owner String representing the file owner (Not implemented)
            @options group String representing the file group (Not implemented)
            @throws IOError if the directory cannot be created.
         */
        native function makeDir(options: Object = null): Void

        /**
            Create a link to a file. Links are not supported on all platforms (such as Windows and VxWORKS).
            @param existing Path to an existing file to link to.
            @param hard Set to true to create a hard link. Otherwise the default is to create a symbolic link.
         */
        native function makeLink(existing: Path, hard: Boolean = false): Void

        /**
            Create a temporary file in the path directory. Creates a new, uniquely named temporary file.
            @returns a new Path object for the temp file.
         */
        native function makeTemp(): Path

        /**
            Get a path after mapping the path directory separator
            @param separator Path directory separator to use
            @return a new Path after mapping separators
         */
        native function map(separator: String): Path

        /** 
            Get the mime type for a path or extension string. The path's extension is used to lookup the corresponding
            mime type.
            @returns The mime type string
         */
        native function mimeType(): String

        /**
            When the file was created or last modified. Set to null the file does not exist.
         */
        native function get modified(): Date 

        /**
            Name of the Path as a string. This is the same as toString().
         */
        native function get name(): String 

        /**
            Natural (native) respresentation of the path. This uses the default O/S file system path separator, 
            this is "\" on windows and "/" on unix and is normalized.
         */
        native function get natural(): Path 

        /**
            Normalized representation of the path. This removes "segment/.." and "./" components. Separators are made 
            uniform by converting all separators to be the same as the first separator found in the path. Note: the 
            value is not converted to an absolute path.
         */
        native function get normalize(): Path

        /**
            Open a path and return a File object.
            @params options
            @options mode optional file access mode string. Use "r" for read, "w" for write, "a" for append to existing
                content, "+" never truncate.
            @options permissions optional Posix permissions number mask. Defaults to 0664.
            @options owner String representing the file owner (Not implemented)
            @options group String representing the file group (Not implemented)
            @return a File object which implements the Stream interface.
            @throws IOError if the path or file cannot be created.
         */
        function open(options: Object = null): File
            new File(this, options)

        /**
            Open a file and return a TextStream object.
            @params options Optional options object
            @options mode optional file access mode string. Use "r" for read, "w" for write, "a" for append to existing
                content, "+" never truncate.
            @options encoding Text encoding format
            @options permissions optional Posix permissions number mask. Defaults to 0664.
            @options owner String representing the file owner (Not implemented)
            @options group String representing the file group (Not implemented)
            @return a TextStream object which implements the Stream interface.
            @throws IOError if the path or file cannot be opened or created.
         */
        function openTextStream(options: Object = null): TextStream
            new TextStream(open(options))

        /**
            Open a file and return a BinaryStream object.
            @params options Optional options object
            @param mode optional file access mode with values ored from: Read, Write, Append, Create, Open, Truncate. 
                Defaults to Read.
            @options permissions optional Posix permissions number mask. Defaults to 0664.
            @options owner String representing the file owner (Not implemented)
            @options group String representing the file group (Not implemented)
            @return a BinaryStream object which implements the Stream interface.
            @throws IOError if the path or file cannot be opened or created.
         */
        function openBinaryStream(options: Object = null): BinaryStream
            new BinaryStream(open(options))

        /**
            The parent directory of path. This may be an absolute path if there are no parent directories in the path.
         */
        native function get parent(): Path

        /**
            The file permissions of a path. This number contains the Posix style permissions value or null if the file 
            does not exist. NOTE: this is not a string representation of an octal posix mask. 
         */
        native function get perms(): Number

        /** */
        native function set perms(perms: Number): Void

        /**
            The path in a portable (like Unix) representation. This uses "/" separators. The value is is normalized and 
            the separators are mapped to "/".
         */
        native function get portable(): Path 

        /**
            Read the file contents as a ByteArray. This method opens the file, reads the contents, closes the file and
                returns a new ByteArray with the contents.
            @return A ByteArray
            @throws IOError if the file cannot be read
            @example:
                var b: ByteArray = Path("/tmp/a.txt").readBytes()
         */
        function readBytes(): ByteArray {
            let file: File = File(this).open()
            result = file.readBytes()
            file.close()
            return result
        }

        /**
            Read the file contents as an array of lines. Each line is a string. This method opens the file, reads the 
                contents and closes the file.
            @return An array of strings.
            @throws IOError if the file cannot be read
            @example:
                for each (line in Path("/tmp/a.txt").readLines())
         */
        function readLines(): Array {
            let stream: TextStream = TextStream(open(this))
            result = stream.readLines()
            stream.close()
            return result
        }

        /**
            Read the file contents as a string. This method opens the file, reads the contents and closes the file.
            @return A string.
            @throws IOError if the file cannot be read
            @example:
                data = Path("/tmp/a.txt").readString()
         */
        function readString(): String {
            let file: File = open(this)
            result = file.readString()
            file.close()
            return result
        }

        /**
            Read the file contents as an XML object.  This method opens the file, reads the contents and closes the file.
            @return An XML object
            @throws IOError if the file cannot be read
         */
        function readXML(): XML {
            let file: File = open(this)
            let data = file.readString()
            file.close()
            if (data == null) {
                return null
            }
            return new XML(data)
        }

        /**
            That path in a form relative to the application's current working directory. The value is normalized.
         */
        native function get relative(): Path

        /**
            Delete the file associated with the Path object. If this is a directory without contents it will be removed.
            @throws IOError if the file exists and could not be deleted.
         */
        native function remove(): Void 

        /**
            Removes a directory and its contents
            If the path is a directory, this call will remove all subdirectories and their contents and finally the
            directory itself. If the directory does not exist, this call does not error and does nothing.
            @throws IOError if the directory exists and cannot be removed.
         */
        function removeAll(): Void {
            for each (f in find('*')) {
                f.remove()
            }
            remove()
        }

        /**
            Rename a file. If the new path exists it is removed before the rename.
            @param target New name of the path. Can be either a Path or String.
            @throws IOError if the original file does not exist or cannot be renamed.
         */
        native function rename(target: Object): Void
        
        /**
            Replace the path extension and return a new path.
            @return A new path with joined extension.
         */
        function replaceExt(ext: String): Path
            this.trimExt().joinExt(ext)

        /**
            Resolve paths relative to this path
            Resolve each other path and return the last path resolved. Each path is resolved by joined the 
            directory containing the prior path to the next path under consideration. If the next path is an 
            absolute path, it is used unmodified. The effect is to find the given paths with a virtual current
            directory set to the directory containing the prior path.
            @return A new Path object that is resolved against the prior path. 
         */
        native function resolve(...otherPaths): Path

        /**
            Compare two paths test if they represent the same file
            @param other Other path to compare with
            @return True if the paths represent the same underlying filename
         */
        native function same(other: Object): Boolean

        /**
            The path separator for this path. This will return the first valid path separator used by the path
            or the default file system path separator if the path has no separators. On Windows, a path may contain
            "/" and "\" separators.  This will be set to a string containing the first separator found in the path.
            Will typically be either "/" or "/\\" depending on the path, platform and file system.
            Use $natural or $portable to create a new path with different path separators.
         */
        native function get separator(): String

        /**
            Size of the file associated with this Path object. Set to number of bytes in the file or -1 if the size
             determination failed.
         */
        native function get size(): Number 

        /**
            Return true if the path starts with the given prefix
            @param prefix String prefix to compare with the path.
            @return true if the path does begin with the prefix
         */
        function startsWith(prefix: String): Boolean
            this.toString().startsWith(prefix) 

        /**
            Convert the path to a JSON string. 
            @return a JSON string representing the path.
         */
        native override function toJSON(): String

        /**
            Convert the path to lower case
            @return a new Path mapped to lower case
         */
        function toLower(): Path
            new Path(name.toString().toLower())

        /**
            Return the path as a string. The path is not translated.
            @return a string representing the path.
         */
        native override function toString(): String

        /**
            Trim a pattern from the end of the path
            NOTE: this does a case-sensitive match
            @return a Path containing the trimmed path name
         */
        function trimEnd(pat: String): Path {
            if (name.endsWith(pat)) {
                loc = name.lastIndexOf(pat)
                if (loc >= 0) {
                    return new Path(name.slice(0, loc))
                }
            }
            return this
        }

        /**
            Trim a file extension from a path
            @return a Path with no extension
         */
        native function trimExt(): Path

        /**
            Trim a pattern from the start of the path
            NOTE: this does a case-sensitive match
            @return a Path containing the trimmed path name
         */
        function trimStart(pat: String): Path {
            if (name.startsWith(pat)) {
                return new Path(name.slice(pat.length))
            }
            return this
        }

        /**
            Reduce the size of the file by truncating it. 
            @param size The new size of the file. If the truncate argument is greater than or equal to the 
                current file size nothing happens.
            @throws IOError if the truncate failed.
         */
        native function truncate(size: Number): Void

        /**
            Write the file contents. This method opens the file, writes the contents and closes the file.
            @param args The data to write to the file. Data is serialized in before writing. Note that numbers will not 
                be written in a cross platform manner. If that is required, use the BinaryStream class to write Numbers.
            @throws IOError if the data cannot be written
         */
        function write(...args): Void {
            var file: File = new File(this, { mode: "w", permissions: 0644 })
            try {
                for each (item in args) {
                    if (item is String) {
                        file.write(item)
                    } else {
                        file.write(serialize(item))
                    }
                }
            } 
            catch (es) {
                throw new IOError("Can't write to file")
            }
            finally {
                file.close()
            }
        }
    }
}

/*
    @copy   default
    
    Copyright (c) Embedthis Software LLC, 2003-2010. All Rights Reserved.
    Copyright (c) Michael O'Brien, 1993-2010. All Rights Reserved.
    
    This software is distributed under commercial and open source licenses.
    You may use the GPL open source license described below or you may acquire 
    a commercial license from Embedthis Software. You agree to be fully bound 
    by the terms of either license. Consult the LICENSE.TXT distributed with 
    this software for full details.
    
    This software is open source; you can redistribute it and/or modify it 
    under the terms of the GNU General Public License as published by the 
    Free Software Foundation; either version 2 of the License, or (at your 
    option) any later version. See the GNU General Public License for more 
    details at: http://www.embedthis.com/downloads/gplLicense.html
    
    This program is distributed WITHOUT ANY WARRANTY; without even the 
    implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
    
    This GPL license does NOT permit incorporating this software into 
    proprietary programs. If you are unable to comply with the GPL, you must
    acquire a commercial license to use this software. Commercial licenses 
    for this software and support services are available from Embedthis 
    Software at http://www.embedthis.com 
    
    @end
 */
/************************************************************************/
/*
 *  End of file "../src/es/io/Path.es"
 */
/************************************************************************/



/************************************************************************/
/*
 *  Start of file "../src/es/io/TextStream.es"
 */
/************************************************************************/

/*
 *  TextStream.es -- TextStream class
 *
 *  Copyright (c) All Rights Reserved. See details at the end of the file.
 */

module ejs.io {

    /**
     *  TextStreams interpret data as a stream of Unicode characters. They provide methods to read and write data
     *  in various text encodings and to read/write lines of text appending appropriate system dependent new line 
     *  terminators. TextStreams can be stacked upon other Streams such as files, byte arrays, sockets, or Http objects.
     *  @spec ejs
     *  @stability evolving
     */
    class TextStream implements Stream {

        use default namespace public

        /** 
            Text encoding formats for use with $encoding 
            @hide
         */
        static const LATIN1: String = "latin1"

        /** 
            Text encoding formats for use with $encoding 
            @hide
         */
        static const UTF_8: String = "utf-8"

        /** 
            Text encoding formats for use with $encoding 
            @hide
         */
        static const UTF_16: String = "utf-16"

        /**
         *  System dependent newline terminator
         */
        private var newline: String = "\n"

        /*
         *  Data input and output buffers
         */
        private var inbuf: ByteArray

        private var format: String = UTF_8

        /*
         *  Provider Stream
         */
        private var nextStream: Stream

        /**
         *  Create a text filter stream. A Text filter stream must be stacked upon a stream source such as a File.
         *  @param stream stream data source/sink to stack upon.
         */
        function TextStream(stream: Stream) {
            if (stream == null) {
                throw new ArgError("Must supply a Stream argument")
            }
            inbuf = new ByteArray
            inbuf.input = fill
            nextStream = stream
            if (Config.OS == "WIN") {
                newline = "\r\n"
            }
        }

        /**
         *  The number of bytes available to read
         */
        function get available(): Number
            inbuf.available

        /**
         *  Close the input stream and free up all associated resources.
         */
        function close(graceful: Boolean = true): Void {
            inbuf.flush(graceful)
            nextStream.close(graceful)
        }

        /**
         *  The current text encoding.
         *  Not implemented
         *  @returns the current text encoding as a string.
         *  @hide
         */
        function get encoding(): String
            format

        /**
         *  Set the current text encoding.
         *  Not implemented
         *  @param encoding string containing the current text encoding. Supported encodings are: utf-8.
         *  @hide
         */
        function set encoding(encoding: String = UTF_8): Void {
            format = encoding
        }

        /*
         *  Fill the input buffer from upstream
         *  @returns The number of new characters added to the input bufer
         */
        private function fill(): Number {
            inbuf.compact()
            return nextStream.read(inbuf, -1)
        }

        /**
         *  Flush the stream and the underlying file data. Will block while flushing. Note: may complete before
         *  the data is actually written to disk.
         *  @param graceful If true, write all pending data.
         */
        function flush(graceful: Boolean = true): Void {
            inbuf.flush(graceful)
            if (!(nextStream is ByteArray)) {
                nextStream.flush(graceful)
            }
        }

        /**
         *  Read characters from the stream into the supplied byte array. This routine is used by upper streams to read
         *  data from the text stream as raw bytes.
         *  @param buffer Destination byte array for the read data.
         *  @param offset Offset in the byte array to place the data. If the offset is -1, then data is
         *      appended to the buffer write $position which is then updated. 
         *  @param count Number of bytes to read. 
         *  @returns a count of characters actually read
         *  @throws IOError if an I/O error occurs.
         */
        function read(buffer: ByteArray, offset: Number = 0, count: Number = -1): Number {
            let total = 0
            if (count < 0) {
                count = Number.MaxValue
            }
            if (offset < 0) {
                buffer.reset()
            } else {
                buffer.flush()
            }
            let where = buffer.writePosition
            while (count > 0) {
                if (inbuf.available == 0) {
                    if (fill() <= 0) {
                        if (total == 0) {
                            return null
                        }
                        break
                    }
                }
                let len = count.min(inbuf.available)
                len = len.min(buffer.length - where)
                if (len == 0) break
                len = buffer.copyIn(where, inbuf, inbuf.readPosition, len)
                inbuf.readPosition += len
                where += len
                total += len
                count -= len
            }
            buffer.writePosition += total
            return total
        }

        /**
         *  Read a line from the stream.
         *  @returns A string containing the next line without the newline character. Return null on eof.
         *  @throws IOError if an I/O error occurs.
         */
        function readLine(): String {
            if (inbuf.available == 0 && fill() <= 0) {
                return null
            }
			// All systems strip both \n and \r\n
			let nl = "\r\n"
            while (true) {
                let nlchar = nl.charCodeAt(nl.length - 1)
                for (let i = inbuf.readPosition; i < inbuf.writePosition; i++) {
                    if (inbuf[i] == nlchar) {
                        if (nl.length == 2 && i > inbuf.readPosition && nl.charCodeAt(0) == inbuf[i-1]) {
							result = inbuf.readString(i - inbuf.readPosition - 1)
							inbuf.readPosition += 2
                        } else {
							result = inbuf.readString(i - inbuf.readPosition)
							inbuf.readPosition++
						}
                        return result
                    }
                }
                if (fill() <= 0) {
                    /*
                     *  Missing a line terminator, so return any last portion of text.
                     */
                    if (inbuf.available) {
                        return inbuf.readString(inbuf.available)
                    }
                }
            }
            return null
        }

        /**
         *  Read a required number of lines of data from the stream.
         *  @param numLines of lines to read. Defaults to read all lines.
         *  @returns Array containing the read lines.
         *  @throws IOError if an I/O error occurs.
         */
        function readLines(numLines: Number = -1): Array {
            var result: Array
            if (numLines <= 0) {
                result = new Array
                numLines = Number.MaxValue
            } else {
                result = new Array(numLines)
            }
            for (let i in numLines) {
                if ((line = readLine()) == null) {
                    if (i == 0) {
                        return null
                    }
                    break
                }
                result[i] = line
            }
            return result
        }

        /**
         *  Read a string from the stream. 
         *  @param count of bytes to read. Returns the entire stream contents if count is -1.
         *  @returns a string
         *  @throws IOError if an I/O error occurs.
         */
        function readString(count: Number = -1): String
            inbuf.readString(count)

        /**
         *  Write characters to the stream.
         *  endpoint can accept more data.
         *  @param data String to write. 
         *  @returns The total number of elements that were written.
         *  @throws IOError if there is an I/O error.
         */
        function write(...data): Number
            nextStream.write(data)

        /**
         *  Write text lines to the stream. The text line is written after appending the system text newline character or
         *  characters. 
         *  @param lines Text lines to write.
         *  @return The number of characters written or -1 if unsuccessful.
         *  @throws IOError if the file could not be written.
         */
        function writeLine(...lines): Number {
            let written = 0
            for each (let line in lines) {
                var count = line.length
                written += nextStream.write(line)
                written += nextStream.write(newline)
            }
            return written
        }
    }
}


/*
 *  @copy   default
 *  
 *  Copyright (c) Embedthis Software LLC, 2003-2010. All Rights Reserved.
 *  Copyright (c) Michael O'Brien, 1993-2010. All Rights Reserved.
 *  
 *  This software is distributed under commercial and open source licenses.
 *  You may use the GPL open source license described below or you may acquire 
 *  a commercial license from Embedthis Software. You agree to be fully bound 
 *  by the terms of either license. Consult the LICENSE.TXT distributed with 
 *  this software for full details.
 *  
 *  This software is open source; you can redistribute it and/or modify it 
 *  under the terms of the GNU General Public License as published by the 
 *  Free Software Foundation; either version 2 of the License, or (at your 
 *  option) any later version. See the GNU General Public License for more 
 *  details at: http://www.embedthis.com/downloads/gplLicense.html
 *  
 *  This program is distributed WITHOUT ANY WARRANTY; without even the 
 *  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
 *  
 *  This GPL license does NOT permit incorporating this software into 
 *  proprietary programs. If you are unable to comply with the GPL, you must
 *  acquire a commercial license to use this software. Commercial licenses 
 *  for this software and support services are available from Embedthis 
 *  Software at http://www.embedthis.com 
 *  
 *  @end
 */
/************************************************************************/
/*
 *  End of file "../src/es/io/TextStream.es"
 */
/************************************************************************/



/************************************************************************/
/*
 *  Start of file "../src/es/io/XMLHttp.es"
 */
/************************************************************************/

/**
    XMLHttp.es -- XMLHttp class
 *
    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

module ejs.io {

    /**
        XMLHttp compatible method to retrieve HTTP data
        This code is prototype and is not yet supported
        @hide
        @stability prototype
     */
    class XMLHttp {

        use default namespace public

        private var hp: Http = new Http
        private var state: Number = 0
        private var response: ByteArray

        /** readyState values */
        static const Uninitialized = 0              

        /** readyState values */
        static const Open = 1

        /** readyState values */
        static const Sent = 2

        /** readyState values */
        static const Receiving = 3

        /** readyState values */
        static const Loaded = 4

        /**
            Call back function for when the HTTP state changes.
         */
        public var onreadystatechange: Function

        /**
            Abort the connection
         */
        function abort(): void
            hp.close

        /**
            The underlying Http object.
            @spec ejs
         */
        function get http() : Http
            hp

        /**
            The readystate value. This value can be compared with the XMLHttp constants: Uninitialized, Open, Sent,
            Receiving or Loaded Set to: Uninitialized = 0, Open = 1, Sent = 2, Receiving = 3, Loaded = 4
         */
        function get readyState() : Number
            state

        /**
            HTTP response body as a string.
         */
        function get responseText(): String
            hp.response

        /**
            HTTP response payload as an XML document. Set to an XML object that is the root of the HTTP request 
            response data.
         */
        function get responseXML(): XML
            XML(hp.response)

        /**
            Not implemented. Only for ActiveX on IE
            @hide
         */
        function get responseBody(): String {
            throw new Error("Unsupported API")
            return ""
        }

        /**
            The HTTP status code. Set to an integer Http status code between 100 and 600.
         */
        function get status(): Number
            hp.code

        /**
            HTTP status code message
         */
        function get statusText() : String
            hp.codeString

        /**
            Return the response headers
            @returns a string with the headers catenated together.
         */
        function getAllResponseHeaders(): String {
            let result: String = ""
            for (key in hp.headers) {
                result = result.concat(key + ": " + hp.headers[key] + '\n')
            }
            return result
        }

        /**
            Return a response header. Not yet implemented.
            @param key The name of the response key to be returned.
            @returns the header value as a string
         */
        function getResponseHeader(key: String)
            header(key)

        /**
            Open a connection to the web server using the supplied URL and method.
            @param method HTTP method to use. Valid methods include "GET", "POST", "PUT", "DELETE", "OPTIONS" and "TRACE"
            @param url URL to invoke
            @param async If true, don't block after issuing the requeset. By defining an $onreadystatuschange callback 
                function, the request progress can be monitored. NOTE: async mode is not supported. All calls will block.
            @param user Optional user name if authentication is required.
            @param password Optional password if authentication is required.
         */
        function open(method: String, url: String, async: Boolean = false, user: String = null, password: String = null): Void {
            hp.method = method
            hp.uri = url
            if (user && password) {
                hp.setCredentials(user, password)
            }
            hp.callback = callback
            response = new ByteArray(System.Bufsize, 1)

            hp.connect()
            state = Open
            notify()

            if (!async || 1) {
                let timeout = 5   1000
                let when: Date = new Date
                while (state != Loaded && when.elapsed < timeout) {
                    App.serviceEvents(1, timeout)
                }
            }
        }

        /**
            Send data with the request.
            @param content Data to send with the request.
         */
        function send(content: String): Void {
/*
            if (hp.callback == null) {
                throw new IOError("Can't call send in sync mode")
            }
*/
            hp.write(content)
        }

        /**
            Set an HTTP header with the request
            @param key Key value for the header
            @param value Value of the header
            @example:
                setRequestHeader("Keep-Alive", "none")
         */
        function setRequestHeader(key: String, value: String): Void
            hp.addHeader(key, value, 1)

        /*
            Http callback function
         */
        private function callback (e: Event) {
            if (e is HttpError) {
                notify()
                return
            }
            let hp: Http = e.data
            let count = hp.read(response)
            state = (count == 0) ? Loaded : Receiving
            notify()
        }

        /*
            Invoke the user's state change handler
         */
        private function notify() {
            if (onreadystatechange) {
                onreadystatechange()
            }
        }
    }
}


/*
    @copy   default
    
    Copyright (c) Embedthis Software LLC, 2003-2010. All Rights Reserved.
    Copyright (c) Michael O'Brien, 1993-2010. All Rights Reserved.
    
    This software is distributed under commercial and open source licenses.
    You may use the GPL open source license described below or you may acquire 
    a commercial license from Embedthis Software. You agree to be fully bound 
    by the terms of either license. Consult the LICENSE.TXT distributed with 
    this software for full details.
    
    This software is open source; you can redistribute it and/or modify it 
    under the terms of the GNU General Public License as published by the 
    Free Software Foundation; either version 2 of the License, or (at your 
    option) any later version. See the GNU General Public License for more 
    details at: http://www.embedthis.com/downloads/gplLicense.html
    
    This program is distributed WITHOUT ANY WARRANTY; without even the 
    implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
    
    This GPL license does NOT permit incorporating this software into 
    proprietary programs. If you are unable to comply with the GPL, you must
    acquire a commercial license to use this software. Commercial licenses 
    for this software and support services are available from Embedthis 
    Software at http://www.embedthis.com 
    
    @end
 */
/************************************************************************/
/*
 *  End of file "../src/es/io/XMLHttp.es"
 */
/************************************************************************/



/************************************************************************/
/*
 *  Start of file "../src/es/sys/App.es"
 */
/************************************************************************/

/*
 *  App.es -- Application configuration and control. (Really controlling the interpreter's environment)
 *
 *  Copyright (c) All Rights Reserved. See details at the end of the file.
 */

module ejs.sys {

    /**
     *  Application configuration state. The App class is a singleton class object. The App class is accessed via
     *  the App global type object. It provides  methods to interrogate and control the applications environment including
     *  the current working directory, application command line arguments, path to the application's executable and
     *  input and output streams.
     *  @spec ejs
     *  @stability evolving
     */
    native class App {

        use default namespace public

        /**
         *  Application command line arguments. Set to an array containing each of the arguments. If the ejs command is 
         *      invoked as "ejs script arg1 arg2", then args[0] will be "script", args[1] will be "arg1" etc.
         */
        native static function get args(): Array

        /**
         *  The application's current directory
         */
        native static function get dir(): Path

        /**
         *  Change the application's Working directory
         *  @param value The path to the new working directory
         */
        native static function chdir(value: Object): Void

        /**
         *  The directory containing the application executable
         */
        native static function get exeDir(): Path

        /**
         *  The application executable path
         */
        native static function get exePath(): Path

        /**
         *  The application's standard error file stream
         */
        native static function get errorStream(): Stream

        /**
            Stop the program and exit.
            @param status The optional exit code to provide the environment. If running inside the ejs command program,
                the status is used as process exit status.
         */
        native static function exit(status: Number = 0): void

        /**
         *  Get an environment variable.
         *  @param name The name of the environment variable to retrieve.
         *  @return The value of the environment variable or null if not found.
         */
        native static function getenv(name: String): String

        /**
         *  The application's standard input file stream
         */
        native static function get inputStream(): Stream

        /**
         *  Application name.  Set to a single word, lower case name for the application.
         */
        static function get name(): String
            Config.Product

        /**
         *  Control whether an application will exit when global scripts have completed. Setting this to true will cause
         *  the application to continue servicing events until the $exit method is explicitly called. The default 
         *  application setting of noexit is false.
         *  @param exit If true, the application will exit when the last script completes.
         */
        native static function noexit(exit: Boolean = true): void

        /**
         *  The application's standard output file stream
         */
        native static function get outputStream(): Stream

        /**
         *  Update an environment variable.
         *  @param name The name of the environment variable to retrieve.
         *  @param value The new value to define for the variable.
         */
        native static function putenv(name: String, value: String): Void

        /** 
            The current module search path . Set to a delimited searchPath string. Warning: This will be changed to an
            array of paths in a future release.
            @stability prototype.
         */
        native static function get searchPath(): String

        /** 
            @duplicate App.searchPath
            Setting a search path will preserve certain system dependant paths that must be present.
            @param path Search path
         */
        native static function set searchPath(path: String): Void

        /**
         *  Service events
         *  @param count Count of events to service. Defaults to unlimited.
         *  @param timeout Timeout to block waiting for an event in milliseconds before returning. If an event occurs, the
         *      call returns immediately.
         */
        native static function serviceEvents(count: Number = -1, timeout: Number = -1): Void

        /**
         *  Set an environment variable.
         *  @param env The name of the environment variable to set.
         *  @param value The new value.
         *  @return True if the environment variable was successfully set.
         */
        # FUTURE
        native static function setEnv(name: String, value: String): Boolean

        /**
         *  Sleep the application for the given number of milliseconds
         *  @param delay Time in milliseconds to sleep. Set to -1 to sleep forever.
         */
        native static function sleep(delay: Number = -1): Void

        /**
         *  Application title name. Multi-word, Camel Case name for the application suitable for display.
         */
        static static function get title(): String
            Config.Title

        /**
         *  Application version string. Set to a version string of the format Major.Minor.Patch-Build. For example: 1.1.2-3.
         */
        static static function get version(): String
            Config.Version
    }
}


/*
 *  @copy   default
 *  
 *  Copyright (c) Embedthis Software LLC, 2003-2010. All Rights Reserved.
 *  Copyright (c) Michael O'Brien, 1993-2010. All Rights Reserved.
 *  
 *  This software is distributed under commercial and open source licenses.
 *  You may use the GPL open source license described below or you may acquire 
 *  a commercial license from Embedthis Software. You agree to be fully bound 
 *  by the terms of either license. Consult the LICENSE.TXT distributed with 
 *  this software for full details.
 *  
 *  This software is open source; you can redistribute it and/or modify it 
 *  under the terms of the GNU General Public License as published by the 
 *  Free Software Foundation; either version 2 of the License, or (at your 
 *  option) any later version. See the GNU General Public License for more 
 *  details at: http://www.embedthis.com/downloads/gplLicense.html
 *  
 *  This program is distributed WITHOUT ANY WARRANTY; without even the 
 *  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
 *  
 *  This GPL license does NOT permit incorporating this software into 
 *  proprietary programs. If you are unable to comply with the GPL, you must
 *  acquire a commercial license to use this software. Commercial licenses 
 *  for this software and support services are available from Embedthis 
 *  Software at http://www.embedthis.com 
 *  
 *  @end
 */
/************************************************************************/
/*
 *  End of file "../src/es/sys/App.es"
 */
/************************************************************************/



/************************************************************************/
/*
 *  Start of file "../src/es/sys/Cmd.es"
 */
/************************************************************************/

/*
 *  Cmd.es - Cmd class
 *
 *  Copyright (c) All Rights Reserved. See details at the end of the file.
 */

module ejs.sys {

    /**
     *  The Cmd class supports invoking other programs on the same system. This class is prototype and will likely
     *  change in the next release.
     *  @spec ejs
     *  @stability prototype
     */
    class Cmd {

        use default namespace public

        /**
         *  Run a command using the system command shell and wait for completion. This supports pipelines.
         *  @param cmdline Command or program to execute
         *  @returns The command output from it's standard output.
         *  @throws IOError if the command exits with non-zero status. The exception object will contain the command's
         *      standard error output. 
         */
        static function sh(cmdline: String, data: String = null): String
            run(("/bin/sh -c \"" + cmdline.replace(/\\/g, "\\\\") + "\"").trim('\n'), data)

        /**
         *  Execute a command/program.
         *  @param cmdline Command or program to execute
         *  @param data Optional data to write to the command on it's standard input. Not implemented.
         *  @returns The command output from it's standard output.
         *  @throws IOError if the command exits with non-zero status. The exception object will contain the command's
         *      standard error output. 
         */
        static function run(cmdline: String, data: String = null): String
            System.run(cmdline)

        /**
         *  Execute a command and detach. This will not capture output nor will it wait for the command to complete
         *  @param cmdline Command or program to execute
         *  @returns The commands process ID
         */
        static function daemon(cmdline: String): Number 
            System.daemon(cmdline)

        /**
         *  Run a command and don't capture output. Output and errors go to the existing stdout
         *  @hide
         */
        static function runx(cmdline: String): String
            System.runx(cmdline)
    }

    /**
     *  Data event issued to the callback function.
     */
    # FUTURE
    class CmdDataEvent extends Event {
        /**
         *  Mask of pending events. Set to include $Read and/or $Write values.
         */
        public var eventMask: Number
    }

    /**
     *  Error event issued to the callback function if any errors occur dcmdlineng an Cmd request.
     */
    # FUTURE
    class CmdErrorEvent extends Event {
    }

}


/*
 *  @copy   default
 *  
 *  Copyright (c) Embedthis Software LLC, 2003-2010. All Rights Reserved.
 *  Copyright (c) Michael O'Brien, 1993-2010. All Rights Reserved.
 *  
 *  This software is distributed under commercial and open source licenses.
 *  You may use the GPL open source license described below or you may acquire 
 *  a commercial license from Embedthis Software. You agree to be fully bound 
 *  by the terms of either license. Consult the LICENSE.TXT distributed with 
 *  this software for full details.
 *  
 *  This software is open source; you can redistribute it and/or modify it 
 *  under the terms of the GNU General Public License as published by the 
 *  Free Software Foundation; either version 2 of the License, or (at your 
 *  option) any later version. See the GNU General Public License for more 
 *  details at: http://www.embedthis.com/downloads/gplLicense.html
 *  
 *  This program is distributed WITHOUT ANY WARRANTY; without even the 
 *  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
 *  
 *  This GPL license does NOT permit incorporating this software into 
 *  proprietary programs. If you are unable to comply with the GPL, you must
 *  acquire a commercial license to use this software. Commercial licenses 
 *  for this software and support services are available from Embedthis 
 *  Software at http://www.embedthis.com 
 *  
 *  @end
 */
/************************************************************************/
/*
 *  End of file "../src/es/sys/Cmd.es"
 */
/************************************************************************/



/************************************************************************/
/*
 *  Start of file "../src/es/sys/Compat.es"
 */
/************************************************************************/

/*
 *  Compat.es -- Compatibility with other JS engines
 *
 *  Copyright (c) All Rights Reserved. See details at the end of the file.
 */

module ejs.sys {

    use default namespace public

    /** @hide */
    function gc(): Void
        GC.run 

    /** @hide */
    function readFile(path: String, encoding: String = null): String
        Path(path).readString()
}

/*
 *  @copy   default
 *  
 *  Copyright (c) Embedthis Software LLC, 2003-2010. All Rights Reserved.
 *  Copyright (c) Michael O'Brien, 1993-2010. All Rights Reserved.
 *  
 *  This software is distributed under commercial and open source licenses.
 *  You may use the GPL open source license described below or you may acquire 
 *  a commercial license from Embedthis Software. You agree to be fully bound 
 *  by the terms of either license. Consult the LICENSE.TXT distributed with 
 *  this software for full details.
 *  
 *  This software is open source; you can redistribute it and/or modify it 
 *  under the terms of the GNU General Public License as published by the 
 *  Free Software Foundation; either version 2 of the License, or (at your 
 *  option) any later version. See the GNU General Public License for more 
 *  details at: http://www.embedthis.com/downloads/gplLicense.html
 *  
 *  This program is distributed WITHOUT ANY WARRANTY; without even the 
 *  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
 *  
 *  This GPL license does NOT permit incorporating this software into 
 *  proprietary programs. If you are unable to comply with the GPL, you must
 *  acquire a commercial license to use this software. Commercial licenses 
 *  for this software and support services are available from Embedthis 
 *  Software at http://www.embedthis.com 
 *  
 *  @end
 */
/************************************************************************/
/*
 *  End of file "../src/es/sys/Compat.es"
 */
/************************************************************************/



/************************************************************************/
/*
 *  Start of file "../src/es/sys/Config.es"
 */
/************************************************************************/

/*
 *  Config.es - Configuration settings from ./configure
 *
 *  Copyright (c) Embedthis Software LLC, 2003-2010. All Rights Reserved.
 */

module ejs.sys {

    /* NOTE: These values are updated at run-time by src/types/sys/ejsConfig.c */

    /**
     *  Config class providing settings for various "configure" program settings.
     *  @spec ejs
     *  @stability evolving
     */
    native class Config extends Object {

        use default namespace public

        /**
         *  True if a debug build
         */
        static const Debug: Boolean

        /**
         *  CPU type (eg. i386, ppc, arm)
         */
        static const CPU: String

        /**
         *  Build with database (SQLite) support
            @hide
         */
        static const DB: Boolean

        /**
         *  Build with E4X support
            @hide
         */
        static const E4X: Boolean

        /**
         *  Build with floating point support
            @hide
         */
        static const Floating: Boolean

        /**
         *  Build with HTTP client support 
            @hide
         */
        static const Http: Boolean

        /**
         *  Language specification level. (ecma|plus|fixed)
            @hide
         */
        static const Lang: String

        /**
         *  Build with legacy API support
            @hide
         */
        static const Legacy: Boolean

        /**
         *  Build with multithreading support
         */
        static const Multithread: Boolean

        /**
         *  Number type
            @hide
         */
        static const NumberType: String

        /**
         *  Operating system version. One of: WIN, LINUX, MACOSX, FREEBSD, SOLARIS
         */
        static const OS: String

        /**
         *  Ejscript product name. Single word name.
         */
        static const Product: String

        /**
         *  Regular expression support.
         *  @hide
         */
        static const RegularExpressions: Boolean

        /**
         *  Ejscript product title. Multiword title.
         */
        static const Title: String

        /**
         *  Ejscript version. Multiword title. Format is Major.Minor.Patch-Build For example: 1.1.2-1
         */
        static const Version: String

        /**
         *  Installation library directory
         */
        static const LibDir: Path

        /**
         *  Binaries directory
         */
        static const BinDir: Path

        /**
         *  Modules directory
         */
        static const ModDir: Path
    }
}

/************************************************************************/
/*
 *  End of file "../src/es/sys/Config.es"
 */
/************************************************************************/



/************************************************************************/
/*
 *  Start of file "../src/es/sys/Debug.es"
 */
/************************************************************************/

/*
 *  Debug.es -- Debug class
 *
 *  Copyright (c) All Rights Reserved. See details at the end of the file.
 */

module ejs.sys {

    /**
     *  Debug configuration class. Singleton class containing the application's debug configuration.
     *  @spec ejs
     *  @stability prototype
     */
    # FUTURE
    class Debug {

        use default namespace public

        /**
         *  Break to the debugger. Suspend execution and break to the debugger.
         */ 
        native function breakpoint(): void

        /**
         *  The current debug mode. This property is read-write. Setting mode to true will put the application in debug 
         *  mode. When debug mode is enabled, the runtime will typically suspend timeouts and will take other actions 
         *  to make debugging easier.
         *  @hide
         */
        native function get mode(): Boolean

        /**
         *  @duplicate Debug.mode
         *  @param value True to turn debug mode on or off.
         *  @hide
         */
        native function set mode(value: Boolean): void
    }
}


/*
 *  @copy   default
 *  
 *  Copyright (c) Embedthis Software LLC, 2003-2010. All Rights Reserved.
 *  Copyright (c) Michael O'Brien, 1993-2010. All Rights Reserved.
 *  
 *  This software is distributed under commercial and open source licenses.
 *  You may use the GPL open source license described below or you may acquire 
 *  a commercial license from Embedthis Software. You agree to be fully bound 
 *  by the terms of either license. Consult the LICENSE.TXT distributed with 
 *  this software for full details.
 *  
 *  This software is open source; you can redistribute it and/or modify it 
 *  under the terms of the GNU General Public License as published by the 
 *  Free Software Foundation; either version 2 of the License, or (at your 
 *  option) any later version. See the GNU General Public License for more 
 *  details at: http://www.embedthis.com/downloads/gplLicense.html
 *  
 *  This program is distributed WITHOUT ANY WARRANTY; without even the 
 *  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
 *  
 *  This GPL license does NOT permit incorporating this software into 
 *  proprietary programs. If you are unable to comply with the GPL, you must
 *  acquire a commercial license to use this software. Commercial licenses 
 *  for this software and support services are available from Embedthis 
 *  Software at http://www.embedthis.com 
 *  
 *  @end
 */
/************************************************************************/
/*
 *  End of file "../src/es/sys/Debug.es"
 */
/************************************************************************/



/************************************************************************/
/*
 *  Start of file "../src/es/sys/GC.es"
 */
/************************************************************************/

/*
 *  GC.es -- Garbage collector class
 *
 *  Copyright (c) All Rights Reserved. See details at the end of the file.
 */

module ejs.sys {

    /**
     *  Garbage collector control class. Singleton class to control operation of the Ejscript garbage collector.
     *  @spec ejs
     *  @stability evolving
     */
    native class GC {

        use default namespace public

        /**
         *  Is the garbage collector is enabled. Enabled by default.
         */
        native static function get enabled(): Boolean

        /**
         *  @duplicate GC.enabled
         *  @param on Set to true to enable the collector.
         */
        native static function set enabled(on: Boolean): Void

        /**
         *  The quota of work to perform before the GC will be invoked. Set to the number of work units that will 
         *  trigger the GC to run. This roughly corresponds to the number of allocated objects.
         */
        native static function get workQuota(): Number

        /**
         *  @duplicate GC.workQuota
         *  @param quota The number of work units that will trigger the GC to run. This roughly corresponds to the number
         *  of allocated objects.
         */
        native static function set workQuota(quota: Number): Void

        /**
         *  Run the garbage collector and reclaim memory allocated to objects and properties that are no longer reachable. 
         *  When objects and properties are freed, any registered destructors will be called. The run function will run 
         *  the garbage collector even if the $enable property is set to false. 
         *  @param deep If set to true, will collect from all generations. The default is to collect only the youngest
         *      geneartion of objects.
         */
        native static function run(deep: Boolean = false): void

    }
}

/*
 *  @copy   default
 *  
 *  Copyright (c) Embedthis Software LLC, 2003-2010. All Rights Reserved.
 *  Copyright (c) Michael O'Brien, 1993-2010. All Rights Reserved.
 *  
 *  This software is distributed under commercial and open source licenses.
 *  You may use the GPL open source license described below or you may acquire 
 *  a commercial license from Embedthis Software. You agree to be fully bound 
 *  by the terms of either license. Consult the LICENSE.TXT distributed with 
 *  this software for full details.
 *  
 *  This software is open source; you can redistribute it and/or modify it 
 *  under the terms of the GNU General Public License as published by the 
 *  Free Software Foundation; either version 2 of the License, or (at your 
 *  option) any later version. See the GNU General Public License for more 
 *  details at: http://www.embedthis.com/downloads/gplLicense.html
 *  
 *  This program is distributed WITHOUT ANY WARRANTY; without even the 
 *  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
 *  
 *  This GPL license does NOT permit incorporating this software into 
 *  proprietary programs. If you are unable to comply with the GPL, you must
 *  acquire a commercial license to use this software. Commercial licenses 
 *  for this software and support services are available from Embedthis 
 *  Software at http://www.embedthis.com 
 *  
 *  @end
 */
/************************************************************************/
/*
 *  End of file "../src/es/sys/GC.es"
 */
/************************************************************************/



/************************************************************************/
/*
 *  Start of file "../src/es/sys/Logger.es"
 */
/************************************************************************/

/*
    Logger.es - Log file control class
 *
    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

module ejs.sys {

    /**
        Logger objects provide a convenient and consistent method to capture and store logging information from 
        applications. The verbosity and scope of the logging can be changed at start-up time. Logs can be sent
        to various output and storage facilities (console, disk files, etc.) 
      
        A logger may have a "parent" logger in order to create hierarchies of loggers for better logging control 
        and granularity. For example, a logger can be created for each class in a package with all such loggers 
        having a single parent. Loggers can send log messages to their parent and inherit their parent's log level. 
        This allows for easier control of verbosity and scope of logging.  
      
        A logger may have a "filter", an arbitrary function, that returns true or false depending on whether a 
        specific message should be logged or not. 
        @spec ejs
        @stability prototype
     */
    # FUTURE
    namespace BIG_SPACE

    # FUTURE
    class Logger {

        use default namespace public

        /**
            Logging level for inherit level from parent.
         */
        static const Inherit: Number = -1

        /**
            Logging level for no logging.
         */
        static const Off: Number = 0

        /**
            Logging level for most serious errors.
         */
        static const Error: Number = 1

        /**
            Logging level for warnings.
         */
        static const Warn: Number = 2

        /**
            Logging level for informational messages.
         */
        static const Info: Number = 3

        /**
            Logging level for configuration output.
         */
        static const Config: Number = 4

        /**
            Logging level to output all messages.
         */
        static const All: Number = 5

        /**
            Do not output messages to any device.
         */
        static const None: Number = 0

        /**
            Output messages to the console.
         */
        static const Console: Number = 0x1

        /**
            Output messages to a file.
         */
        static const LogFile: Number = 0x2

        /**
            Output messages to an O/S event log.
         */
        static const EventLog: Number = 0x4

        /**
            Output messages to a in-memory store.
         */
        static const MemLog: Number = 0x8

        /**
            Logger constructor.
            The Logger constructor can create different types of loggers based on the three (optional) arguments. 
            The logging level can be set in the constructor and also changed at run-time. Where the logger output 
            goes (e.g. console or file) is statically set. A logger may have a parent logger to provide hierarchical 
            mapping of loggers to the code structure.
            @param name Loggers are typically named after the namespace package or class they are associated with.
            @param level Optional enumerated integer specifying the verbosity.
            @param output Optional output device(s) to send messages to.
            @param parent Optional parent logger.
            @example:
                var log = new Logger("name", 5, LogFile)
                log(2, "message")
         */
        native function Logger(name: String, level: Number = 0, output: Number = LogFile, parent: Logger = null)

        /**
            Get the filter function for a logger.
            @return The filter function.
         */
        native function get filter(): Function

        /**
            Set the filter function for this logger. The filter function is called with the following signature:
         *
                function filter(log: Logger, level: Number, msg: String): Boolean
         *
            @param function The filter function must return true or false.
         */
        native function set filter(filter: Function): void

        /**
            Get the verbosity setting (level) of this logger.
            @return The level.
         */
        native function get level(): Number

        /**
            Set the output level of this logger. (And all child loggers who have their logging level set to Inherit.)
            @param level The next logging level (verbosity).
         */
        native function set level(level: Number): void

        /**
            Get the name of this logger.
            @return The string name.
         */
        native function get name(): String

        /**
            Set the name for this logger.
            @param name An optional string name.
         */
        native function set name(name: String): void

        /**
            Get the devices this logger sends messages to.
            @return The different devices OR'd together.
         */
        native function get output(): Number

        /**
            Set the output devices for this logger.
            @param name Logically OR'd list of devices.
         */
        native function set output(ouput: Number): void

        /**
            Get the parent of this logger.
            @return The parent logger.
         */
        native function get parent(): Logger

        /**
            Set the parent logger for this logger.
            @param parent A logger.
         */
        native function set parent(parent: Logger): void

        /**
            Record a message via a logger. The message level will be compared to the logger setting to determine 
            whether it will be output to the devices or not. Also, if the logger has a filter function set that 
            may filter the message out before logging.
            @param level The level of the message.
            @param msg The string message to log.
         */
        native function log(level: Number, msg: String): void

        /**
            Convenience method to record a configuration change via a logger.
            @param msg The string message to log.
         */
        native function config(msg: String): void

        /**
            Convenience method to record an error via a logger.
            @param msg The string message to log.
         */
        native function error(msg: String): void

        /**
            Convenience method to record an informational message via a logger.
            @param msg The string message to log.
         */
        native function info(msg: String): void

        /**
            Convenience method to record a warning via a logger.
            @param msg The string message to log.
         */
        native function warn(msg: String): void
    }
}


/*
    @copy   default
    
    Copyright (c) Embedthis Software LLC, 2003-2010. All Rights Reserved.
    Copyright (c) Michael O'Brien, 1993-2010. All Rights Reserved.
    
    This software is distributed under commercial and open source licenses.
    You may use the GPL open source license described below or you may acquire 
    a commercial license from Embedthis Software. You agree to be fully bound 
    by the terms of either license. Consult the LICENSE.TXT distributed with 
    this software for full details.
    
    This software is open source; you can redistribute it and/or modify it 
    under the terms of the GNU General Public License as published by the 
    Free Software Foundation; either version 2 of the License, or (at your 
    option) any later version. See the GNU General Public License for more 
    details at: http://www.embedthis.com/downloads/gplLicense.html
    
    This program is distributed WITHOUT ANY WARRANTY; without even the 
    implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
    
    This GPL license does NOT permit incorporating this software into 
    proprietary programs. If you are unable to comply with the GPL, you must
    acquire a commercial license to use this software. Commercial licenses 
    for this software and support services are available from Embedthis 
    Software at http://www.embedthis.com 
    
    @end
 */
/************************************************************************/
/*
 *  End of file "../src/es/sys/Logger.es"
 */
/************************************************************************/



/************************************************************************/
/*
 *  Start of file "../src/es/sys/Memory.es"
 */
/************************************************************************/

/*
 *  Memory.es -- Memory statistics
 *
 *  Copyright (c) All Rights Reserved. See details at the end of the file.
 */

module ejs.sys {

    /**
     *  Singleton class to monitor and report on memory allocation and usage.
     *  @spec ejs
     *  @stability evolving
     */
    native class Memory {

        use default namespace public

        /**
         *  Total heap memory currently allocated by the application in bytes. This includes memory currently in use and 
         *  also memory that has been freed but is still retained by the application for future use. It does not include 
         *  code, static data or stack memory. If you require these, use the $resident call.
         */
        native static function get allocated(): Number

        /**
         *  Memory redline callback. When the memory redline limit is exceeded, the callback will be invoked. 
         *  If no callback is defined and the redline limit is exceeded, a MemoryError exception is thrown. This callback
         *  enables the application detect low memory conditions before they become critical and to recover by freeing 
         *  memory or to gracefully exit. The callback is invoked with the following signature:
         *      function callback(size: Number, total: Number): Void
         *  @param fn Callback function to invoke when the redline limit is exceeded. While the callback is active
         *      subsequent invocations of the callback are suppressed.
         */
        native static function set callback(fn: Function): Void

        /** @hide */
        native static function get callback(): Void

        /**
            Maximum amount of heap memory the application may use in bytes. 
            This defines the upper limit for heap memory usage 
            by the entire hosting application. If this limit is reached, subsequent memory allocations will fail and 
            a $MemoryError exception will be thrown. Setting it to zero will allow unlimited memory allocations up 
            to the system imposed maximum. If $redline is defined and non-zero, the redline callback will be invoked 
            when the $redline is exceeded.
         */
        native static function get maximum(): Number

        /**
            @duplicate Memory.maximum
         *  @param value New maximum value in bytes
         */
        native static function set maximum(value: Number): Void

        /**
         *  Peak memory ever used by the application in bytes. This statistic is the maximum value ever attained by 
         *  $allocated. 
         */
        native static function get peak(): Number
        
        /**
            Memory redline value in bytes. When the memory redline limit is exceeded, the redline $callback will be invoked. 
            If no callback is defined, a MemoryError exception is thrown. The redline limit enables the application detect 
            low memory conditions before they become critical and to recover by freeing memory or to gracefully exit. 
            Note: the redline applies to the entire hosting application.
         */
        native static function get redline(): Number

        /**
         *  @duplicate Memory.redline
         *  @param value New memory redline limit in bytes
         */
        native static function set redline(value: Number): Void

        /**
         *  Application's current resident set in bytes. This is the total memory used to host the application and includes 
         *  all the the application code, data and heap. It is measured by the O/S.
         */
        native static function get resident(): Number

        /**
         *  Peak stack size ever used by the application in bytes. 
         */
        native static function get stack(): Number
        
        /**
         *  System RAM. This is the total amount of RAM installed in the system in bytes
         */
        native static function get system(): Number
        
        /**
         *  Prints memory statistics to stdout. This is primarily used during development for performance measurement.
         */
        native static function stats(): void
    }
}


/*
 *  @copy   default
 *  
 *  Copyright (c) Embedthis Software LLC, 2003-2010. All Rights Reserved.
 *  Copyright (c) Michael O'Brien, 1993-2010. All Rights Reserved.
 *  
 *  This software is distributed under commercial and open source licenses.
 *  You may use the GPL open source license described below or you may acquire 
 *  a commercial license from Embedthis Software. You agree to be fully bound 
 *  by the terms of either license. Consult the LICENSE.TXT distributed with 
 *  this software for full details.
 *  
 *  This software is open source; you can redistribute it and/or modify it 
 *  under the terms of the GNU General Public License as published by the 
 *  Free Software Foundation; either version 2 of the License, or (at your 
 *  option) any later version. See the GNU General Public License for more 
 *  details at: http://www.embedthis.com/downloads/gplLicense.html
 *  
 *  This program is distributed WITHOUT ANY WARRANTY; without even the 
 *  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
 *  
 *  This GPL license does NOT permit incorporating this software into 
 *  proprietary programs. If you are unable to comply with the GPL, you must
 *  acquire a commercial license to use this software. Commercial licenses 
 *  for this software and support services are available from Embedthis 
 *  Software at http://www.embedthis.com 
 *  
 *  @end
 */
/************************************************************************/
/*
 *  End of file "../src/es/sys/Memory.es"
 */
/************************************************************************/



/************************************************************************/
/*
 *  Start of file "../src/es/sys/System.es"
 */
/************************************************************************/

/*
 *  System.es - System class
 *
 *  Copyright (c) All Rights Reserved. See details at the end of the file.
 */

module ejs.sys {
    /**
     *  System is a utility class providing methods to interact with the operating system.
     *  @spec ejs
     *  @stability prototype
     */
    native class System {

        use default namespace public

        public static const Bufsize: Number = 1024

        /**
         *  The fully qualified system hostname
         */
        native static function get hostname(): String

        /**
         *  Execute a command/program.
         *  @param cmd Command or program to execute
         *  @return a text stream connected to the programs standard output.
         *  @throws IOError if the command exits with non-zero status. 
         */
        native static function run(cmd: String): String

        /**
            Run a program without capturing stdout.
            @hide
         */
        native static function runx(cmd: String): Void

        /** @hide */
        native static function daemon(cmd: String): Number

        /**
            Run a command using the system command shell. This allows pipelines and also works better cross platform on
            Windows Cygwin.
            @hide
         */
        static function sh(args): String {
            return System.run("/bin/sh -c \"" + args.replace(/\\/g, "\\\\") + "\"").trim('\n')
        }

        /**  TEMP deprecated @hide */
        static function cmd(args): String
            sh(args)

        /**  TEMP @hide */
        native static function exec(args): String
    }
}


/*
 *  @copy   default
 *  
 *  Copyright (c) Embedthis Software LLC, 2003-2010. All Rights Reserved.
 *  Copyright (c) Michael O'Brien, 1993-2010. All Rights Reserved.
 *  
 *  This software is distributed under commercial and open source licenses.
 *  You may use the GPL open source license described below or you may acquire 
 *  a commercial license from Embedthis Software. You agree to be fully bound 
 *  by the terms of either license. Consult the LICENSE.TXT distributed with 
 *  this software for full details.
 *  
 *  This software is open source; you can redistribute it and/or modify it 
 *  under the terms of the GNU General Public License as published by the 
 *  Free Software Foundation; either version 2 of the License, or (at your 
 *  option) any later version. See the GNU General Public License for more 
 *  details at: http://www.embedthis.com/downloads/gplLicense.html
 *  
 *  This program is distributed WITHOUT ANY WARRANTY; without even the 
 *  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
 *  
 *  This GPL license does NOT permit incorporating this software into 
 *  proprietary programs. If you are unable to comply with the GPL, you must
 *  acquire a commercial license to use this software. Commercial licenses 
 *  for this software and support services are available from Embedthis 
 *  Software at http://www.embedthis.com 
 *  
 *  @end
 */
/************************************************************************/
/*
 *  End of file "../src/es/sys/System.es"
 */
/************************************************************************/



/************************************************************************/
/*
 *  Start of file "../src/es/sys/Unix.es"
 */
/************************************************************************/

/*
 *  Unix.es -- Unix compatibility functions
 *
 *  Copyright (c) All Rights Reserved. See details at the end of the file.
 */

module ejs.sys {

    use default namespace public

    /**
     *  Get the base name of a file. Returns the base name portion of a file name. The base name portion is the 
     *  trailing portion without any directory elements.
     *  @return A string containing the base name portion of the file name.
     */
    function basename(path: String): Path
        Path(path).basename
    
    /**
     *  Change the current working directory
     *  @param dir Directory String or path to change to
     */
    function chdir(dir: Object): Void
        App.chdir(dir)

    /**
     *  Set the permissions of a file or directory
     *  @param path File or directory to modify
     *  @param perms New Posix style permission mask
     */
    function chmod(path: String, perms: Number): Void
        Path(path).perms = perms

    /**
     *  Close the file and free up all associated resources.
     *  @param file Open file object previously opened via $open or $File
     *  @param graceful if true, then close the file gracefully after writing all pending data.
     */
    function close(file: File, graceful: Boolean = true): Void
        file.close(graceful)

    /**
     *  Copy a file. If the destination file already exists, the old copy will be overwritten as part of the copy operation.
     *  @param fromPath Original file to copy.
     *  @param toPath New destination file path name.
     *  @throws IOError if the copy is not successful.
     */
    function cp(fromPath: String, toPath: String): void
        Path(fromPath).copy(toPath) 

    /**
     *  Get the directory name portion of a file. The dirname name portion is the leading portion including all 
     *  directory elements and excluding the base name. On some systems, it will include a drive specifier.
     *  @return A string containing the directory name portion of the file name.
     */
    function dirname(path: String): Path
        Path(path).dirname

    /**
     *  Does a file exist. Return true if the specified file exists and can be accessed.
     *  @param path Filename path to examine.
     *  @return True if the file can be accessed
     */
    function exists(path: String): Boolean
        Path(path).exists

    /**
     *  Get the file extension portion of the file name. The file extension is the portion starting with the last "."
     *  in the path. It thus includes "." as the first charcter.
     *  @param path Filename path to examine
     *  @return String containing the file extension. It includes "." as the first character.
     */
    function extension(path: String): String
        Path(path).extension

    /**
     *  Return the free space in the file system.
     *  @return The number of 1M blocks (1024 * 1024 bytes) of free space in the file system.
     */
    function freeSpace(path: String = null): Number
        FileSystem(path).freeSpace()

    /**
     *  Is a file a directory. Return true if the specified path exists and is a directory
     *  @param path Directory path to examine.
     *  @return True if the file can be accessed
     */
    function isDir(path: String): Boolean
        Path(path).isDir

    /**
     *  Kill a process
     *  @param pid Process ID to kill
     *  @param signal Signal number to issue to kill the process
     */
    function kill(pid: Number, signal: Number = 2): Void {
        if (Config.OS == "WIN") {
            Cmd.run("/bin/kill -f -" + signal + " " + pid)
        } else {
            Cmd.run("/bin/kill -" + signal + " " + pid)
        }
    }

    /**
     *  Get a list of files in a directory. The returned array contains the base file name portion only.
     *  @param path Directory path to enumerate.
     *  @param enumDirs If set to true, then dirList will include sub-directories in the returned list of files.
     *  @return An Array of strings containing the filenames in the directory.
     */
    function ls(path: String = ".", enumDirs: Boolean = false): Array
        Path(path).files(enumDirs)

    /**
     *  Find matching files. Files are listed in a depth first order.
     *  @param path Starting path from which to find matching files.
     *  @param glob Glob style Pattern that files must match. This is similar to a ls() style pattern.
     *  @param recurse Set to true to examine sub-directories. 
     *  @return Return a list of matching files
     */
    function find(path: Object, glob: String, recurse: Boolean = true): Array {
        let result = []
        if (path is Array) {
            let paths = path
            for each (path in paths) {
                result += Path(path).find(glob, recurse)
            }
        } else {
            result += Path(path).find(glob, recurse)
        }
        return result
    }

    /**
     *  Make a new directory. Makes a new directory and all required intervening directories. If the directory 
     *  already exists, the function returns without throwing an exception.
     *  @param path Filename path to use.
     *  @param permissions Optional posix permissions mask number. e.g. 0664.
     *  @throws IOError if the directory cannot be created.
     */
    function mkdir(path: String, permissions: Number = 0755): void
        Path(path).makeDir({permissions: permissions})
    
    /**
     *  Rename a file. If the new file name exists it is removed before the rename.
     *  @param fromFile Original file name.
     *  @param toFile New file name.
     *  @throws IOError if the original file does not exist or cannot be renamed.
     */
    function mv(fromFile: String, toFile: String): void
        Path(fromFile).rename(toFile)

    /**
     *  Open or create a file
     *  @param path Filename path to open
     *  @param mode optional file access mode with values ored from: Read, Write, Append, Create, Open, Truncate. 
     *      Defaults to Read.
     *  @param permissions optional permissions. Defaults to App.permissions
     *  @return a File object which implements the Stream interface
     *  @throws IOError if the path or file cannot be opened or created.
     */
    function open(path: String, mode: Number = Read, permissions: Number = 0644): File
        new File(path, { mode: mode, permissions: permissions})

    /**
     *  Get the current working directory
     *  @return A Path containing the current working directory
     */
    function pwd(): Path
        App.dir

    /**
     *  Read data bytes from a file and return a byte array containing the data.
     *  @param file Open file object previously opened via $open or $File
        @param count Number of bytes to read
     *  @return A byte array containing the read data
     *  @throws IOError if the file could not be read.
     */
    function read(file: File, count: Number): ByteArray
        file.read(count)

    /**
     *  Remove a file from the file system.
     *  @param path Filename path to delete.
     *  @param quiet Don't complain if the file does not exist.
     *  @throws IOError if the file exists and cannot be removed.
     */
    function rm(path: Path): void {
        if (path.isDir) {
            throw new ArgError(path.toString() + " is a directory")
        } 
        Path(path).remove()
    }

    /**
     *  Removes a directory. This can remove a directory and its contents.  
     *  @param path Filename path to remove.
     *  @param contents If true, remove the directory contents including files and sub-directories.
     *  @throws IOError if the directory exists and cannot be removed.
     */
    function rmdir(path: Path, contents: Boolean = false): void {
        if (contents) {
            Path(path).removeAll()
        } else {
            Path(path).remove()
        }
    }

    /**
     *  Create a temporary file. Creates a new, uniquely named temporary file.
     *  @param directory Directory in which to create the temp file.
     *  @returns a closed File object after creating an empty temporary file.
     */
    function tempname(directory: String = null): File
        FileSystem.makeTemp(directory)

    /**
     *  Write data to the file. If the stream is in sync mode, the write call blocks until the underlying stream or 
     *  endpoint absorbes all the data. If in async-mode, the call accepts whatever data can be accepted immediately 
     *  and returns a count of the elements that have been written.
     *  @param file Open file object previously opened via $open or $File
     *  @param items The data argument can be ByteArrays, strings or Numbers. All other types will call serialize
     *  first before writing. Note that numbers will not be written in a cross platform manner. If that is required, use
     *  the BinaryStream class to write Numbers.
     *  @returns the number of bytes written.  
     *  @throws IOError if the file could not be written.
     */
    function write(file: File, ...items): Number
        file.write(items)
}

/*
 *  @copy   default
 *  
 *  Copyright (c) Embedthis Software LLC, 2003-2010. All Rights Reserved.
 *  Copyright (c) Michael O'Brien, 1993-2010. All Rights Reserved.
 *  
 *  This software is distributed under commercial and open source licenses.
 *  You may use the GPL open source license described below or you may acquire 
 *  a commercial license from Embedthis Software. You agree to be fully bound 
 *  by the terms of either license. Consult the LICENSE.TXT distributed with 
 *  this software for full details.
 *  
 *  This software is open source; you can redistribute it and/or modify it 
 *  under the terms of the GNU General Public License as published by the 
 *  Free Software Foundation; either version 2 of the License, or (at your 
 *  option) any later version. See the GNU General Public License for more 
 *  details at: http://www.embedthis.com/downloads/gplLicense.html
 *  
 *  This program is distributed WITHOUT ANY WARRANTY; without even the 
 *  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
 *  
 *  This GPL license does NOT permit incorporating this software into 
 *  proprietary programs. If you are unable to comply with the GPL, you must
 *  acquire a commercial license to use this software. Commercial licenses 
 *  for this software and support services are available from Embedthis 
 *  Software at http://www.embedthis.com 
 *  
 *  @end
 */
/************************************************************************/
/*
 *  End of file "../src/es/sys/Unix.es"
 */
/************************************************************************/



/************************************************************************/
/*
 *  Start of file "../src/es/sys/Worker.es"
 */
/************************************************************************/

/*
 *  Worker -- Worker classes
 *
 *  Copyright (c) All Rights Reserved. See details at the end of the file.
 */

module ejs.sys {


    /**
     *  Worker Class. Worker threads are medium-weight thread-based virtual machine instances. They run separate 
     *  interpreters with tightly controlled data interchange. This class is currently being specified by the
     *  WebWorker task group (See http://www.whatwg.org/specs/web-workers/current-work/#introduction).
     *  This class is prototype and highly likely to change
     *  @stability prototype.
     *  @spec WebWorker-W3C
     */
    class Worker {
        use default namespace public

        /**
         *  Callback function invoked when the worker exits. 
         *  The "this" object is set to the worker object.
         */
        var onclose: Function

        /**
         *  Callback function to receive incoming messages. This is invoked when postMessage is called in another Worker. 
         *  The "this" object is set to the worker object.
         *  This is invoked as: function (event) { }
         */
        var onmessage: Function

        /**
         *  Callback function to receive incoming error events. This is invoked when the Worker thows an exception. 
         *  The "this" object is set to the worker object.
         */
        var onerror: Function

        /**
         *  Worker name. This name is initialized but workers can update as required.
         *  @spec ejs
         */
        var name: String

        /**
         *  Create a new Worker instance. This call returns an outside worker object for using in the calling interpreter.
         *      Inside the worker interpreter, a corresponding "insdie" worker object is created that is paired to the
         *      outside worker.
         *  @params script Optional path to a script or module to execute. If supplied, then a new Worker instance will
         *      invoke load() to execute the script.
         *  @params options Options hash
         *  @options search Search path
         *  @options name Name of the Worker instance.
         *  @spec WebWorker
         */
        native function Worker(script: Path = null, options: Object = null)

        /**
         *  Load the script. The literal script is compiled as a JavaScript program and loaded and run.
         *  This is similar to the global eval() command but the script is run in its own interpreter and does not
         *  share any data the the invoking interpreter. The result is serialized in the worker and then deserialized
         *  (using JSON) in the current interpreter. The call returns undefined if the timeout expires.
         *  @param script Literal JavaScript program string.
         *  @param timeout If the timeout is non-zero, this call blocks and will return the value of the last expression in
         *      the script. Otherwise, this call will not block and join() can be used to wait for completion. Set the
         *      timeout to -1 to block until the script completes. The default is -1.
         *  @returns The value of the last expression evaluated in the script. Returns undefined if the timeout 
         *      expires before the script completes.
         *  @throws an exception if the script can't be compiled or if it thows a run-time exception.
         *  @spec ejs
         */
        native function eval(script: String, timeout: Number = -1): String

        /**
         *  Exit the worker.
         *  @spec ejs
         */
        native static function exit(): Void

        /**
         *  Wait for Workers to exit
         *  @param workers Set of Workers to wait for. Can be a single Worker object or an array of Workers. If null or 
         *      if the array is empty, then all workers are waited for.
         *  @param timeout Timeout to wait in milliseconds. The value -1 disables the timeout.
         *  @spec ejs
         */
        native static function join(workers: Object = null, timeout: Number = -1): Boolean

        /**
         *  Load and run a script in a dedicated worker thread. 
         *  @params script Filename of a script or module to load and run. 
         *  @param timeout If the timeout is non-zero, this call blocks and will return the value of the last expression in
         *      the script. Otherwise, this call will not block and join() can be used to wait for completion. Set the
         *      timeout to -1 to block until the script completes. The default is to not block.
         *  @spec ejs
         */
        native function load(script: Path, timeout: Number = 0): Void

        /**
         *  Preload the specified script or module file to initialize the worker. This will run a script using the current
         *  thread and will block. To run a worker using its own thread, use load() or Worker(script).
         *  This call will load the script/module and initialize and run global code. The call will block until 
         *  all global code has completed and the script/module is initialized. 
         *  @param path Filename path for the module or script to load. This should include the file extension.
         *  @returns the value of the last expression in the script or module.
         *  @throws an exception if the script or module can't be loaded or initialized or if it thows an exception.
         *  @spec ejs
         */
        native function preload(path: Path): String

        /**
         *  Lookup a Worker by name
         *  @param name Lookup a Worker
         *  @spec ejs
         */
        native static function lookup(name: String): Worker

        /**
         *  Post a message to the Worker's parent
         *  @param data Data to pass to the worker's onmessage callback.
         *  @param ports Not implemented
         */
        native function postMessage(data: Object, ports: Array = null): Void

        /**
         *  Terminate the worker
         */
        native function terminate(): Void

        /**
         *  Wait for receipt of a message
         *  @param timeout Timeout to wait in milliseconds
         *  @stability prototype
         */
        native function waitForMessage(timeout: Number = -1): Boolean
    }


    /*
     *  Globals for inside workers.
     */
    use default namespace "ejs.sys.worker"

    /**
     *  Reference to the Worker object for use inside a worker script
     *  @returns a Worker object
     */
    var self: Worker

    /**
     *  Exit the worker
     *  @spec ejs
     */
    function exit(): Void
        self.exit()

    /**
     *  Post a message to the Worker's parent
     *  @param data Data to pass to the worker's onmessage callback.
     *  @param ports Not implemented
     */
    function postMessage(data: Object, ports: Array = null): Void
        self.postMessage(data, ports)

    /**
     *  The error callback function
     */
    function get onerror(): Function
        self.onerror

    /**
     *  Set the error callback function
     *  @param fun Callback function to receive incoming data from postMessage() calls.
     */
    function set onerror(fun: Function): Void {
        self.onerror = fun
    }

    /**
     *  The callback function configured to receive incoming messages. 
     */
    function get onmessage(): Function
        self.onmessage

    /**
     *  Set the callback function to receive incoming messages. 
     *  @param fun Callback function to receive incoming data from postMessage() calls.
     */
    function set onmessage(fun: Function): Void {
        self.onmessage = fun
    }

    # WebWorker // Only relevant in browsers 
    var location: WorkerLocation

}
/************************************************************************/
/*
 *  End of file "../src/es/sys/Worker.es"
 */
/************************************************************************/



/************************************************************************/
/*
 *  Start of file "../src/es/web/Cache.es"
 */
/************************************************************************/

/**
 *  Cache.es -- Cache class

    Timeouts?
 */

module ejs.web {

    /**
     *  Cache class to store parsed cookie strings. WARNING: this class is a prototype and very likely to change.
     *  @stability prototype
     *  @spec ejs
     *  @hide
     */
    class Cache {

        use default namespace public

        private static var cache: Object = {}

        /**
         *  @options connect Connection string for the cache store
         *  @options timeout Timeout on cache I/O operations
            @hide
         */
        native function Cache(options: Object = null)

        /**
            @hide
         */
        native function read(domain: String, key: String): Object

        /**
         *  @options lifetime Preservation time for the key in seconds 
            @hide
         */
        native function write(domain: String, key: String, value: Object, options: Object = {}): Void

        /**
            @hide
         */
        native function remove(domain: String, key: String): Void
    }
}


/*
 *  @copy   default
 *  
 *  Copyright (c) Embedthis Software LLC, 2003-2010. All Rights Reserved.
 *  Copyright (c) Michael O'Brien, 1993-2010. All Rights Reserved.
 *  
 *  This software is distributed under commercial and open source licenses.
 *  You may use the GPL open source license described below or you may acquire 
 *  a commercial license from Embedthis Software. You agree to be fully bound 
 *  by the terms of either license. Consult the LICENSE.TXT distributed with 
 *  this software for full details.
 *  
 *  This software is open source; you can redistribute it and/or modify it 
 *  under the terms of the GNU General Public License as published by the 
 *  Free Software Foundation; either version 2 of the License, or (at your 
 *  option) any later version. See the GNU General Public License for more 
 *  details at: http://www.embedthis.com/downloads/gplLicense.html
 *  
 *  This program is distributed WITHOUT ANY WARRANTY; without even the 
 *  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
 *  
 *  This GPL license does NOT permit incorporating this software into 
 *  proprietary programs. If you are unable to comply with the GPL, you must
 *  acquire a commercial license to use this software. Commercial licenses 
 *  for this software and support services are available from Embedthis 
 *  Software at http://www.embedthis.com 
 *
 *  @end
 */
/************************************************************************/
/*
 *  End of file "../src/es/web/Cache.es"
 */
/************************************************************************/



/************************************************************************/
/*
 *  Start of file "../src/es/web/connectors/GoogleConnector.es"
 */
/************************************************************************/

/**
 *	GoogleConnector.es -- View connector for the Google Visualization library
 */

module ejs.web {

    /**
        @hide
     */
	class GoogleConnector {

        use default namespace "ejs.web"

        function GoogleConnector(controller) {
            // this.controller = controller
        }

        private var nextId: Number = 0

        private function scriptHeader(kind: String, id: String): Void {
            write('<script type="text/javascript" src="http://www.google.com/jsapi"></script>')
            write('<script type="text/javascript">')
            write('  google.load("visualization", "1", {packages:["' + kind + '"]});')
            write('  google.setOnLoadCallback(' + 'draw_' + id + ');')
        }

        /**
            @hide
         *  @duplicate ejs.web::View.table
         */
		function table(data, options: Object): Void {
            var id: String = "GoogleTable_" + nextId++

			if (data == null || data.length == 0) {
				write("<p>No Data</p>")
				return
			}
            let columns: Array = options["columns"]

            scriptHeader("table", id)
            
            write('  function ' + 'draw_' + id + '() {')
			write('    var data = new google.visualization.DataTable();')

            let firstLine: Object = data[0]
            if (columns) {
                if (columns[0] != "id") {
                    columns.insert(0, "id")
                }
                for (let i = 0; i < columns.length; ) {
                    if (firstLine[columns[i]]) {
                        i++
                    } else {
                        columns.remove(i, i)
                    }
                }
            } else {
                columns = []
                for (let name in firstLine) {
                    columns.append(name)
                }
            }

            for each (name in columns) {
                write('    data.addColumn("string", "' + name.toPascal() + '");')
			}
			write('    data.addRows(' + data.length + ');')

			for (let row: Object in data) {
                let col: Number = 0
                for each (name in columns) {
                    write('    data.setValue(' + row + ', ' + col + ', "' + data[row][name] + '");')
                    col++
                }
            }

            write('    var table = new google.visualization.Table(document.getElementById("' + id + '"));')

            let goptions = getOptions(options, { 
                height: null, 
                page: null,
                pageSize: null,
                showRowNumber: null,
                sort: null,
                title: null,
                width: null, 
            })

            write('    table.draw(data, ' + serialize(goptions) + ');')

            if (options.click) {
                write('    google.visualization.events.addListener(table, "select", function() {')
                write('        var row = table.getSelection()[0].row;')
                write('        window.location = "' + view.makeUrl(options.click, "", options) + '?id=" + ' + 
                    'data.getValue(row, 0);')
                write('    });')
            }

            write('  }')
            write('</script>')

            write('<div id="' + id + '"></div>')
		}

        /**
            @hide
            @duplicate ejs.web::View.chart
         */
		function chart(grid: Array, options: Object): Void {
            var id: String = "GoogleChart_" + nextId++

			if (grid == null || grid.length == 0) {
				write("<p>No Data</p>")
				return
			}

            let columns: Array = options["columns"]

            scriptHeader("piechart", id)
            
            write('  function ' + 'draw_' + id + '() {')
			write('    var data = new google.visualization.DataTable();')

			let firstLine: Object = grid[0]
            let col: Number = 0
            let dataType: String = "string"
			for (let name: String in firstLine) {
                if  (columns && columns.contains(name)) {
                    write('    data.addColumn("' + dataType + '", "' + name.toPascal() + '");')
                    col++
                    if (col >= 2) {
                        break
                    }
                    dataType = "number"
                }
			}
			write('    data.addRows(' + grid.length + ');')

			for (let row: Object in grid) {
                let col2: Number = 0
				for (let name2: String in grid[row]) {
                    if  (columns && columns.contains(name2)) {
                        if (col2 == 0) {
                            write('    data.setValue(' + row + ', ' + col2 + ', "' + grid[row][name2] + '");')
                        } else if (col2 == 1) {
                            write('    data.setValue(' + row + ', ' + col2 + ', ' + grid[row][name2] + ');')
                        }
                        //  else break. 
                        col2++
                    }
                }
            }

            //  PieChart, Table
            write('    var chart = new google.visualization.PieChart(document.getElementById("' + id + '"));')

            let goptions = getOptions(options, { width: 400, height: 400, is3D: true, title: null })
            write('    chart.draw(data, ' + serialize(goptions) + ');')

            write('  }')
            write('</script>')

            write('<div id="' + id + '"></div>')
		}

        /**
         *  Parse an option string
         */
        private function getOptions(options: Object, defaults: Object): Object {
            var result: Object = {}
            for (let word: String in defaults) {
                if (options[word]) {
                    result[word] = options[word]
                } else if (defaults[word]) {
                    result[word] = defaults[word]
                }
            }
            return result
        }

        private function write(str: String): Void {
            view.write(str)
        }
	}
}


/*
 *	@copy	default
 *	
 *	Copyright (c) Embedthis Software LLC, 2003-2010. All Rights Reserved.
 *	Copyright (c) Michael O'Brien, 1993-2010. All Rights Reserved.
 *	
 *	This software is distributed under commercial and open source licenses.
 *	You may use the GPL open source license described below or you may acquire 
 *	a commercial license from Embedthis Software. You agree to be fully bound 
 *	by the terms of either license. Consult the LICENSE.TXT distributed with 
 *	this software for full details.
 *	
 *	This software is open source; you can redistribute it and/or modify it 
 *	under the terms of the GNU General Public License as published by the 
 *	Free Software Foundation; either version 2 of the License, or (at your 
 *	option) any later version. See the GNU General Public License for more 
 *	details at: http://www.embedthis.com/downloads/gplLicense.html
 *	
 *	This program is distributed WITHOUT ANY WARRANTY; without even the 
 *	implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
 *	
 *	This GPL license does NOT permit incorporating this software into 
 *	proprietary programs. If you are unable to comply with the GPL, you must
 *	acquire a commercial license to use this software. Commercial licenses 
 *	for this software and support services are available from Embedthis 
 *	Software at http://www.embedthis.com 
 *	
 *	@end
 */
/************************************************************************/
/*
 *  End of file "../src/es/web/connectors/GoogleConnector.es"
 */
/************************************************************************/



/************************************************************************/
/*
 *  Start of file "../src/es/web/connectors/HtmlConnector.es"
 */
/************************************************************************/

/**
 *	HtmlConnector.es -- Basic HTML control connector
 */

module ejs.web {

    require ejs.db

	/**
	 *	The Html Connector provides bare HTML encoding of Ejscript controls
        @hide
	 */
	class HtmlConnector {

        use default namespace "ejs.web"

        private var nextId: Number = 0
        private var controller: Controller

        function HtmlConnector(controller) {
            this.controller = controller
        }

        /*
         *  Options to implement:
         *      method
         *      update
         *      confirm     JS confirm code
         *      condition   JS expression. True to continue
         *      success
         *      failure
         *      query
         *
         *  Not implemented
         *      submit      FakeFormDiv
         *      complete
         *      before
         *      after
         *      loading
         *      loaded
         *      interactive
         */
        /**
         *  Render an asynchronous (ajax) form.
         *  @param record Initial data
         *  @param url Action to invoke when the form is submitted. Defaults to "create" or "update" depending on 
         *      whether the field has been previously saved.
         *  @param options Optional extra options. See $getOptions for a list of the standard options.
         *  @option url String Use a URL rather than action and controller for the target url.
         */
		function aform(record: Object, url: String, options: Object): Void {
            if (options.id == undefined) {
                options.id = "form"
            }
            onsubmit = ""
            if (options.condition) {
                onsubmit += options.condition + ' && '
            }
            if (options.confirm) {
                onsubmit += 'confirm("' + options.confirm + '"); && '
            }
            onsubmit = '$.ajax({ ' +
                'url: "' + url + '", ' + 
                'type: "' + options.method + '", '

            if (options.query) {
                onsubmit += 'data: ' + options.query + ', '
            } else {
                onsubmit += 'data: $("#' + options.id + '").serialize(), '
            }

            if (options.update) {
                if (options.success) {
                    onsubmit += 'success: function(data) { $("#' + options.update + '").html(data).hide("slow"); ' + 
                        options.success + '; }, '
                } else {
                    onsubmit += 'success: function(data) { $("#' + options.update + '").html(data).hide("slow"); }, '
                }
            } else if (options.success) {
                onsubmit += 'success: function(data) { ' + options.success + '; } '
            }
            if (options.error) {
                onsubmit += 'error: function(data) { ' + options.error + '; }, '
            }
            onsubmit += '}); return false;'

            write('<form action="' + "/User/list" + '"' + getOptions(options) + "onsubmit='" + onsubmit + "' >")
        }

        /*
         *  Extra options:
         *      method
         *      update
         *      confirm     JS confirm code
         *      condition   JS expression. True to continue
         *      success
         *      failure
         *      query
         *
         */
        /** 
         *  Emit an asynchronous (ajax) link to an action. The URL is constructed from the given action and the 
         *      current controller. The controller may be overridden by setting the controller option.
         *  @param text Link text to display
         *  @param url Action to invoke when the link is clicked
         *  @param options Optional extra options. See $getOptions for a list of the standard options.
         *  @option controller String Name of the target controller for the given action
         *  @option url String Use a URL rather than action and controller for the target url.
         */
		function alink(text: String, url: String, options: Object): Void {
            if (options.id == undefined) {
                options.id = "alink"
            }
            onclick = ""
            if (options.condition) {
                onclick += options.condition + ' && '
            }
            if (options.confirm) {
                onclick += 'confirm("' + options.confirm + '"); && '
            }
            onclick = '$.ajax({ ' +
                'url: "' + url + '", ' + 
                'type: "' + options.method + '", '

            if (options.query) {
                'data: ' + options.query + ', '
            }

            if (options.update) {
                if (options.success) {
                    onclick += 'success: function(data) { $("#' + options.update + '").html(data); ' + 
                        options.success + '; }, '
                } else {
                    onclick += 'success: function(data) { $("#' + options.update + '").html(data); }, '
                }
            } else if (options.success) {
                onclick += 'success: function(data) { ' + options.success + '; } '
            }
            if (options.error) {
                onclick += 'error: function(data) { ' + options.error + '; }, '
            }
            onclick += '}); return false;'
            write('<a href="' + options.url + '"' + getOptions(options) + "onclick='" + onclick + "' >" + text + '</a>')
		}

        /**
         *  @duplicate ejs.web::View.button
         */
		function button(value: String, buttonName: String, options: Object): Void {
            write('<input name="' + buttonName + '" type="submit" value="' + value + '"' + getOptions(options) + ' />')
        }

        /**
         *  @duplicate ejs.web::View.buttonLink
         */
		function buttonLink(text: String, url: String, options: Object): Void {
			// write('<a href="' + url + '"><button>' + text + '</button></a>')
			write('<button onclick="window.location=\'' + url + '\';">' + text + '</button></a>')
        }

        /**
            @hide
         */
		function chart(data: Array, options: Object): Void {
            throw 'HtmlConnector control "chart" not implemented.'
		}

        /**
         *  Render an input checkbox. This creates a checkbox suitable for use within an input form. 
         *  @param name Name of the field to display. This is used to create a HTML "name" and "id" attribute for the 
         *      input element. If used inside a model form, it is the field name in the model containing the checkbox
         *      value to display. If used without a model, the value to display should be passed via options.value. 
         *  @param value Value to display
         *  @param submitValue Value to submit if checked. Defaults to "true"
         *  @param options Optional extra options. See $getOptions for a list of the standard options.
         */
		function checkbox(name: String, value: String, submitValue: String, options: Object): Void {
            let checked = (value == submitValue) ? ' checked="yes" ' : ''
            write('<input name="' + name + '" type="checkbox" "' + getOptions(options) + checked + 
                '" value="' + submitValue + '" />')
            write('<input name="' + name + '" type="hidden" "' + getOptions(options) + '" value="" />')
        }

        /**
         *  @duplicate ejs.web::View.endform
         */
		function endform(): Void {
            write('</form>')
        }

        /** 
         *  Emit a flash message area. 
         *  @param kind Kind of flash messages to display. 
         *  @param msg Flash message to display
         *  @param options Optional extra options. See $getOptions for a list of the standard options.
         *  @option retain Number. Number of seconds to retain the message. If <= 0, the message is retained until another
         *      message is displayed. Default is 0.
         *  @example
         *      <% flash("status") %>
         *      <% flash() %>
         *      <% flash(["error", "warning"]) %>
         */
		function flash(kind: String, msg: String, options: Object): Void {
            write('<div' + getOptions(options) + '>' + msg + '</div>\r\n')
            if (kind == "inform") {
                write('<script>$(document).ready(function() {
                        $("div.-ejs-flashInform").animate({opacity: 1.0}, 2000).hide("slow");});
                    </script>')
            }
		}

        /**
         *  Render a form.
         *  @param record Model record to edit
         *  @param url Action to invoke when the form is submitted. Defaults to "create" or "update" depending on 
         *      whether the field has been previously saved.
         *  @param options Optional extra options. See $getOptions for a list of the standard options.
         *  @option url String Use a URL rather than action and controller for the target url.
         */
		function form(record: Object, url: String, options: Object): Void {
            write('<form method="post" action="' + url + '"' + getOptions(options) + ' xonsubmit="ejs.fixCheckboxes();">')
//          write('<input name="id" type="hidden" value="' + record.id + '" />')
        }

        /**
         *  @duplicate ejs.web::View.image
         */
        function image(src: String, options: Object): Void {
			write('<img src="' + src + '"' + getOptions(options) + '/>')
        }

        /**
         *  @duplicate ejs.web::View.label
         */
        function label(text: String, options: Object): Void {
            write('<span ' + getOptions(options) + ' type="' + getTextKind(options) + '">' +  text + '</span>')
        }

        /** 
         *  Emit a link to an action. The URL is constructed from the given action and the current controller. The controller
         *  may be overridden by setting the controller option.
         *  @param text Link text to display
         *  @param action Action to invoke when the link is clicked
         *  @param options Optional extra options. See $getOptions for a list of the standard options.
         *  @option controller String Name of the target controller for the given action
         *  @option url String Use a URL rather than action and controller for the target url.
         */
		function link(text: String, action: String, options: Object): Void {
			write('<a href="' + action + '"' + getOptions(options) + '>' + text + '</a>')
		}

        /**
         *  @duplicate ejs.web::View.extlink
         */
		function extlink(text: String, url: String, options: Object): Void {
			write('<a href="' + url + '"' + getOptions(options) + '>' + text + '</a>')
		}

        /**
         *  Emit a selection list. 
         *  @param field Name of the field to display. This is used to create a HTML "name" and "id" attribute for the 
         *      input element. If used inside a model form, it is the field name in the model containing the list item to
         *      select. If used without a model, the value to select should be passed via options.value. 
         *  @param choices Choices to select from. This can be an array list where each element is displayed and the value 
         *      returned is an element index (origin zero). It can also be an array of array tuples where the first 
         *      tuple entry is the value to display and the second is the value to send to the app. Or it can be an 
         *      array of objects such as those returned from a table lookup. If choices is null, the $field value is 
         *      used to construct a model class name to use to return a data grid containing an array of row objects. 
         *      The first non-id field is used as the value to display.
         *  @param defaultValue Current value
         *  @params options Extra options
         *  Examples:
         *      list("stockId", Stock.stockList) 
         *      list("low", ["low", "med", "high"])
         *      list("low", [["low", "3"], ["med", "5"], ["high", "9"]])
         *      list("low", [{low: 3{, {med: 5}, {high: 9}])
         *      list("Stock Type")                          Will invoke StockType.findAll() to do a table lookup
         */
		function list(field: String, choices: Object, defaultValue: String, options: Object): Void {
            write('<select name="' + field + '" ' + getOptions(options) + '>')
            let isSelected: Boolean
            let i = 0
            for each (choice in choices) {
                if (choice is Array) {
                    isSelected = (choice[0] == defaultValue) ? 'selected="yes"' : ''
                    write('  <option value="' + choice[0] + '"' + isSelected + '>' + choice[1] + '</option>')
                } else {
                    if (choice && choice.id) {
                        for (field in choice) {
                            isSelected = (choice.id == defaultValue) ? 'selected="yes"' : ''
                            if (field != "id") {
                                write('  <option value="' + choice.id + '"' + isSelected + '>' + choice[field] + '</option>')
                                done = true
                                break
                            }
                        }
                    } else {
                        isSelected = (i == defaultValue) ? 'selected="yes"' : ''
                        write('  <option value="' + i + '"' + isSelected + '>' + choice + '</option>')
                    }
                }
                i++
            }
            write('</select>')
        }

        /**
         *  @duplicate ejs.web::View.mail
         */
		function mail(nameText: String, address: String, options: Object): Void  {
			write('<a href="mailto:' + address + '" ' + getOptions(options) + '>' + nameText + '</a>')
		}

        /**
         *  @duplicate ejs.web::View.progress
            @hide
         */
		function progress(initialData: Array, options: Object): Void {
            write('<p>' + initialData + '%</p>')
		}

        //  Emit: <input name ="model.name" id="id" class="class" type="radio" value="text"
        /** 
         *  Emit a radio autton. The URL is constructed from the given action and the current controller. The controller
         *      may be overridden by setting the controller option.
         *  @param name Name of the field to display. This is used to create a HTML "name" and "id" attribute for the 
         *      input element. If used inside a model form, it is the field name in the model containing the radio data to
         *      display. If used without a model, the value to display should be passed via options.value. 
            @param selected Selected option
         *  @param choices Array or object containing the option values. If array, each element is a radio option. If an 
         *      object hash, then they property name is the radio text to display and the property value is what is returned.
         *  @param action Action to invoke when the button is clicked or invoked
         *  @param options Optional extra options. See $getOptions for a list of the standard options.
         *  @option controller String Name of the target controller for the given action
         *  @option value String Name of the option to select by default
         *  @example
         *      radio("priority", ["low", "med", "high"])
         *      radio("priority", {low: 0, med: 1, high: 2})
         *      radio(priority, Message.priorities)
         */
        function radio(name: String, selected: String, choices: Object, options: Object): Void {
            let checked: String
            if (choices is Array) {
                for each (v in choices) {
                    checked = (v == selected) ? "checked" : ""
                    write(v + ' <input type="radio" name="' + name + '"' + getOptions(options) + 
                        ' value="' + v + '" ' + checked + ' />\r\n')
                }
            } else {
                for (item in choices) {
                    checked = (choices[item] == selected) ? "checked" : ""
                    write(item + ' <input type="radio" name="' + name + '"' + getOptions(options) + 
                        ' value="' + choices[item] + '" ' + checked + ' />\r\n')
                }
            }
        }

		/** 
		 *	@duplicate ejs.web::View.script
		 */
		function script(url: String, options: Object): Void {
            write('<script src="' + url + '" type="text/javascript"></script>\r\n')
		}

        /**
         *  @duplicate ejs.web::View.status
            @hide
         */
		function status(data: Array, options: Object): Void {
            write('<p>' + data + '</p>\r\n')
        }

		/** 
		 *	@duplicate ejs.web::View.stylesheet
		 */
		function stylesheet(url: String, options: Object): Void {
            write('<link rel="stylesheet" type="text/css" href="' + url + '" />\r\n')
		}

        /**
         *  @duplicate ejs.web::View.tabs
         */
		function tabs(initialData: Array, options: Object): Void {
            write('<div class="-ejs-tabs">\r\n')
            write('   <ul>\r\n')
            for each (t in initialData) {
                for (name in t) {
                    let url = t[name]
                    write('      <li onclick="window.location=\'' + url + '\'"><a href="' + url + '">' + name + '</a></li>\r\n')
                }
            }
            write('    </ul>')
            write('</div>')
        }

        private function getColumns(data, options: Object): Object {
            let columns
            if (options.columns) {
                if (options.columns is Array) {
                    columns = {}
                    for each (name in options.columns) {
                        columns[name] = name
                    }
                } else {
                    columns = options.columns
                }
            } else {
                /*
                 *  No supplied columns. Infer from data
                 */
                columns = {}
                if (data is Array) {
                    for (let name in data[0]) {
                        if (name == "id" && !options.showId) continue
                        columns[name] = name
                    }
                }
            }
            return columns
        }
    
/*
        private function getSort(columns: Object, options: Object): Array {
            let sort = options.sort || true
            if (!sort) return [-1, 0]
            let sortCol = -1 
            let sortOrder = 0
            if (options.sort) {
                let col = 0
                for (name in columns) {
                    if (name == options.sort) {
                        sortCol = col
                        sortOrder = (options.sortOrder.toLower().contains("asc")) ? 0 : 1
                        break
                    }
                    col++
                }
            }
            if (sortCol < 0) {
                col = 0
                for each (column in columns) {
                    if (column.sort) {
                        sortCol = col
                        sortOrder = (column.sort.toLower().contains("asc")) ? 0 : 1
                        break
                    }
                }
            }
            return [sortCol, sortOrder]
        }
*/

        /**
         *  @duplicate ejs.web::View.table
         */
		function table(data, options: Object = null): Void {
            let originalOptions = options
            let tableId = view.getNextId()

            if (data is Array) {
                if (data.length == 0) {
                    write("<p>No Data</p>")
                    return
                }
            } else if (!(data is Array) && data is Object) {
                data = [data]
			}

            options = (originalOptions && originalOptions.clone()) || {}
            let columns = getColumns(data, options)

            let refresh = options.refresh || 10000
            let sortOrder = options.sortOrder || ""
            let sort = options.sort
            if (sort == undefined) sort = true

            if (!options.ajax) {
                let url = (data is String) ? data : null
                url ||= options.data
                write('  <script type="text/javascript">\r\n' +
                    '   $(function() { $("#' + tableId + '").eTable({ refresh: ' + refresh + 
                    ', sort: "' + sort + '", sortOrder: "' + sortOrder + '"' + 
                    ((url) ? (', url: "' + url + '"'): "") + 
                    '})});\r\n' + 
                    '  </script>\r\n')
                if (data is String) {
                    /* Data is an action method */
                    write('<table id="' + tableId + '" class="-ejs-table"></table>\r\n')
                    return
                }
            } else {
                write('  <script type="text/javascript">$("#' + tableId + '").eTableSetOptions({ refresh: ' + refresh +
                    ', sort: "' + sort + '", sortOrder: "' + sortOrder + '"})' + ';</script>\r\n')
            }
			write('  <table id="' + tableId + '" class="-ejs-table ' + (options.styleTable || "" ) + '">\r\n')

            /*
             *  Table title and column headings
             */
            if (options.showHeader != false) {
                write('    <thead class="' + (options.styleHeader || "") + '">\r\n')
                if (options.title) {
                    if (columns.length < 2) {
                        write('  <tr><td>' + options.title + ' ' + '<img src="' + controller.appUrl + 
                            '/web/images/green.gif" ' + 'class="-ejs-table-download -ejs-clickable" onclick="$(\'#' + 
                            tableId + '\').eTableToggleRefresh();" />\r\n  </td></tr>\r\n')
                    } else {
                        write('  <tr><td colspan="' + (columns.length - 1) + '">' + options.title + 
                            '</td><td class="right">' + '<img src="' + controller.appUrl + '/web/images/green.gif" ' + 
                            'class="-ejs-table-download -ejs-clickable" onclick="$(\'#' + tableId + 
                            '\').eTableToggleRefresh();" />\r\n  </td></tr>\r\n')
                    }
                }
                /*
                 *  Emit column headings
                 */
                if (columns) {
                    write('    <tr>\r\n')
                    for (let name in columns) {
                        if (name == null) continue
                        let header = (columns[name].header) ? (columns[name].header) : name.toPascal()
                        let width = (columns[name].width) ? ' width="' + columns[name].width + '"' : ''
                        write('    <th ' + width + '>' + header + '</th>\r\n')
                    }
                }
                write("     </tr>\r\n    </thead>\r\n")
            }

            let styleBody = options.styleBody || ""
            write('    <tbody class="' + styleBody + '">\r\n')

            let row: Number = 0

			for each (let r: Object in data) {
                let url = null
                let urlOptions = { controller: options.controller, query: options.query }
                if (options.click) {
                    urlOptions.query = (options.query is Array) ? options.query[row] : options.query
                    if (options.click is Array) {
                        if (options.click[row] is String) {
                            url = view.makeUrl(options.click[row], r.id, urlOptions)
                        }
                    } else {
                        url = view.makeUrl(options.click, r.id, urlOptions)
                    }
                }
                let odd = options.styleOddRow || "-ejs-oddRow"
                let even = options.styleOddRow || "-ejs-evenRow"
                styleRow = ((row % 2) ? odd : even) || ""
                if (options.styleRows) {
                    styleRow += " " + (options.styleRows[row] || "")
                }
                if (url) {
                    write('    <tr class="' + styleRow + 
                        '" onclick="window.location=\'' + url + '\';">\r\n')
                } else {
                    write('    <tr class="' + styleRow + '">\r\n')
                }

                let col = 0
				for (name in columns) {
                    if (name == null) {
                        continue
                    }
                    let column = columns[name]
                    let styleCell: String = ""

                    if (options.styleColumns) {
                        styleCell = options.styleColumns[col] || ""
                    }
                    if (column.style) {
                        styleCell += " " + column.style
                    }
                    if (options.styleCells && options.styleCells[row]) {
                        styleCell += " " + (options.styleCells[row][col] || "")
                    }
                    styleCell = styleCell.trim()
                    data = view.getValue(r, name, { render: column.render, formatter: column.formatter } )

                    let align = ""
                    if (column.align) {
                        align = ' align="' + column.align + '"'
                    }
                    let cellUrl
                    if (options.click is Array && options.click[0] is Array) {
                        if (options.query is Array) {
                            if (options.query[0] is Array) {
                                urlOptions.query = options.query[row][col]
                            } else {
                                urlOptions.query = options.query[row]
                            }
                        } else {
                            urlOptions.query = options.query
                        }
                        cellUrl = view.makeUrl(options.click[row][col], r.id, urlOptions)
                    }
					styleCell = styleCell.trim()
                    if (cellUrl) {
                        write('    <td class="' + styleCell + '"' + align + 
                            ' xonclick="window.location=\'' + cellUrl + '\';"><a href="' + cellUrl + '">' + 
                            data + '</a></td>\r\n')
                    } else {
                        write('    <td class="' + styleCell + '"' + align + '>' + data + '</td>\r\n')
                    }
                    col++
				}
                row++
				write('    </tr>\r\n')
			}
			write('    </tbody>\r\n  </table>\r\n')
		}

        //  Emit: <input name ="model.name" id="id" class="class" type="text|hidden|password" value="text"
        /**
         *  Render a text input field as part of a form.
         *  @param name Name of the field to display. This is used to create a HTML "name" and "id" attribute for the 
         *      input element. If used inside a model form, it is the field name in the model containing the text data to
         *      display. If used without a model, the value to display should be passed via options.value. 
            @param value Text to display
         *  @param options Optional extra options. See $getOptions for a list of the standard options.
         *  @option escape Boolean Escape the text before rendering. This converts HTML reserved tags and delimiters into
         *      an encoded form.
         *  @option style String CSS Style to use for the control
         *  @option visible Boolean Make the control visible. Defaults to true.
         *  @examples
         *      <% text("name") %>
         */
        function text(name: String, value: String, options: Object): Void {
            write('<input name="' + name + '" ' + getOptions(options) + ' type="' + getTextKind(options) + 
                '" value="' + value + '" />')
        }

        // Emit: <textarea name ="model.name" id="id" class="class" type="text|hidden|password" value="text"
        /**
         *  Render a text area
         *  @param name Name of the field to display. This is used to create a HTML "name" and "id" attribute for the 
         *      input element. If used inside a model form, it is the field name in the model containing the text data to
         *      display. If used without a model, the value to display should be passed via options.value. 
            @param value Text to display
         *  @option Boolean escape Escape the text before rendering. This converts HTML reserved tags and delimiters into
         *      an encoded form.
         *  @param options Optional extra options. See $getOptions for a list of the standard options.
         *  @option data String URL or action to get data 
         *  @option numCols Number number of text columns
         *  @option numRows Number number of text rows
         *  @option style String CSS Style to use for the control
         *  @option visible Boolean Make the control visible. Defaults to true.
         *  @examples
         *      <% textarea("name") %>
         */
        function textarea(name: String, value: String, options: Object): Void {
            numCols = options.numCols
            if (numCols == undefined) {
                numCols = 60
            }
            numRows = options.numRows
            if (numRows == undefined) {
                numRows = 10
            }
            write('<textarea name="' + name + '" type="' + getTextKind(options) + '" ' + getOptions(options) + 
                ' cols="' + numCols + '" rows="' + numRows + '">' + value + '</textarea>')
        }

        /**
         *  @duplicate ejs.web::View.tree
            @hide
         */
        function tree(initialData: Array, options: Object): Void {
            throw 'HtmlConnector control "tree" not implemented.'
        }

        private function getTextKind(options): String {
            var kind: String

            if (options.password) {
                kind = "password"
            } else if (options.hidden) {
                kind = "hidden"
            } else {
                kind = "text"
            }
            return kind
        }

		private function getOptions(options: Object): String
            view.getOptions(options)

        private function write(str: String): Void
            view.write(str)
	}
}


/*
 *	@copy	default
 *	
 *	Copyright (c) Embedthis Software LLC, 2003-2010. All Rights Reserved.
 *	Copyright (c) Michael O'Brien, 1993-2010. All Rights Reserved.
 *	
 *	This software is distributed under commercial and open source licenses.
 *	You may use the GPL open source license described below or you may acquire 
 *	a commercial license from Embedthis Software. You agree to be fully bound 
 *	by the terms of either license. Consult the LICENSE.TXT distributed with 
 *	this software for full details.
 *	
 *	This software is open source; you can redistribute it and/or modify it 
 *	under the terms of the GNU General Public License as published by the 
 *	Free Software Foundation; either version 2 of the License, or (at your 
 *	option) any later version. See the GNU General Public License for more 
 *	details at: http://www.embedthis.com/downloads/gplLicense.html
 *	
 *	This program is distributed WITHOUT ANY WARRANTY; without even the 
 *	implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
 *	
 *	This GPL license does NOT permit incorporating this software into 
 *	proprietary programs. If you are unable to comply with the GPL, you must
 *	acquire a commercial license to use this software. Commercial licenses 
 *	for this software and support services are available from Embedthis 
 *	Software at http://www.embedthis.com 
 *	
 *	@end
 */
/************************************************************************/
/*
 *  End of file "../src/es/web/connectors/HtmlConnector.es"
 */
/************************************************************************/



/************************************************************************/
/*
 *  Start of file "../src/es/web/Controller.es"
 */
/************************************************************************/

/**
 *  Controller.es -- Ejscript Controller class as part of the MVC framework. Also contains control helpers for views.
 */

module ejs.web {

    require ejs.db

    namespace action = "action"                 /* Namespace for all action methods */

    /**
     *  Current view in the web framework
     *  @spec ejs
     */
    var view: View

    /**
     *  Web framework controller. Part of the Ejscript web MVC framework. Controller objects are not instantiated by
     *  users but are created internally by the web framework.
     *  @spec ejs
     *  @stability prototype
     */
    class Controller {
        /*
         *  Define properties and functions (by default) in the ejs.web namespace so that user controller variables 
         *  don't clash. Override with "public" the specific properties that must be copied to views.
         */
        use default namespace "ejs.web"

        /**
         *  Name of the action being run
         */
        public var actionName:  String 
        private var originalActionName:  String 

        /**
         *  Stores application global data. The application array provides a means to store persistent information 
         *  to be shared across all clients using an application. Objects and variables stored in the application 
         *  array will live until either explicitly deleted or the web server exits. The application array does not 
         *  persist over system reboots. The elements are User defined.
         *  NOTE: Not yet implemented.
         */
        // public var application: Object

        public var absUrl:      String

        /**
         *  Relative URL for the application. Does not include the URL scheme or host name portion.
         */
        public var appUrl:      String

        /**
         *   Lower case controller name 
         */
        public var controllerName: String

        /**
         *  Web application configuration. This is initialized from the config *.ecf files.
         */
        public var config:      Object

        /**
         *  Flash messages to display on the next screen
         *      "inform"        Informational / postitive feedback (note)
         *      "message"       Neutral feedback (reminders, suggestions...)
         *      "warning"       Negative feedback (Warnings and errors)
         *      "error"         Negative errors (Warnings and errors)
         */
        public var flash:       Object

        /**
         *  Physical home directory of the application
         */
        public var home:        String

        /**
         *  Host object
         */
        public var host:        Host

        /**
         *  Form parameters. 
         */
        public var params:      Object

        /**
         *  The request object stores details of the incoming client's request
         */
        public var request:     Request

        /**
         *  The response object stores details of the response going back to the client.
         */
        public var response:    Response

        /**
         *  Stores session state information. The session array will be created automatically if SessionAutoCreate 
         *  is defined or if a session is started via the useSession() or createSession() functions. Sessions are 
         *  shared among requests that come from a single client. This may mean that multiple requests access the 
         *  same session concurrently. Ejscript ensures that such accesses are serialized. The elements are user defined.
         */
        public var session:     Object

        private var isApp:      Boolean
        private var rendered:   Boolean
        private var redirected: Boolean
        private var events:     Dispatcher

        private var _afterFilters: Array
        private var _beforeFilters: Array
        private var _wrapFilters: Array

        /**
         *  Controller initialization. This is specially hand-crafted by the hosting web server so that it runs
         *  before the sub-classing constructors.
         *  @param isApp True if the request if for an MVC application
         *  @param appDir Set to the top level directory containing the application
         *  @param appUrl URL that points to the application
         *  @param session Session state object
         *  @param host Host object
         *  @param request Request object
         *  @param response Response object
         */
        function initialize(isApp: Boolean, appDir: String, appUrl: String, session: Session, host: Host, request: Request, 
                response: Response): Void {

            this.isApp = isApp
            this.home = appDir
            this.session = session
            this.host = host
            this.request = request
            this.response = response
            this.absUrl = appUrl

            /*
             *  this.absUrl = request.url
             *  this.appUrl = Url("/").relative(request.url)
             */
            let url = new Path("")
            let parts = Path(request.url).components
            for (i = 2; i < parts.length; i++) {
                url = url.join("..")
            }
            if (request.url.endsWith("/")) {
                url = url.join("..")
            }
            this.appUrl = url.toString()
            if (this.appUrl == "") {
                this.appUrl = "."
            }
            
            /*
             *  Load application configuration. 
             */
            if (isApp) {
                config = deserialize("{ " + Path(appDir).join("config/config.ecf").readString() + " }")
                config.database = deserialize("{ " + Path(appDir).join("config/database.ecf").readString() + " }")
                config.view = deserialize("{ " + Path(appDir).join("config/view.ecf").readString() + " }")

                let adapter: String = config.database[config.app.mode].adapter
                let dbname: String = config.database[config.app.mode].database

                if (adapter != "" && dbname != "") {
                    db = Database.defaultDatabase = new Database(adapter, Path(appDir).join(dbname))

                    if (config.database[config.app.mode].trace) {
                        db.trace(true)
                    }
                }
            }

            // events = new Dispatcher
            rendered = false
            redirected = false
            params = new Object
            let name: String = Reflect(this).name
            controllerName = name.trim("Controller")
        }

        /** 
         *  Add a cache-control header to direct the browser to not cache the response.
         */
        native function cache(enable: Boolean = true): Void

        /**
         *  Enable session control. This enables session state management for this request and other requests 
         *  from the browser. If a session has not already been created, this call creates a session and sets 
         *  the @sessionID property in the request object. If a session already exists, this call has no effect. 
         *  A cookie containing a session ID is automatically created and sent to the client on the first response 
         *  after creating the session. If SessionAutoCreate is defined in the configuration file, then sessions 
         *  will automatically be created for every web request and your Ejscript web pages do not need to call 
         *  createSession. Multiple requests may be sent from a client's browser at the same time. Ejscript will 
         *  ensure that accesses to the sesssion object are correctly serialized. 
         *  @param timeout Optional timeout for the session in seconds. If ommitted the default timeout is used.
         */
        native function createSession(timeout: Number = 0): Void

        /**
         *  Destroy a session. This call destroys the session state store that is being used for the current client. 
         *  If no session exists, this call has no effect.
         */
        native function destroySession(): Void

        /**
         *  Discard all prior output
         */
        native function discardOutput(): Void

        /** @hide */
        function resetFilters(): Void {
            _beforeFilters = null
            _afterFilters = null
            _wrapFilters = null
        }

        /** @hide */
        function beforeFilter(fn, options: Object = null): Void {
            if (_beforeFilters == null) {
                _beforeFilters = []
            }
            _beforeFilters.append([fn, options])
        }

        /** @hide */
        function afterFilter(fn, options: Object = null): Void {
            if (_afterFilters == null) {
                _afterFilters = []
            }
            _afterFilters.append([fn, options])
        }

        /** @hide */
        function wrapFilter(fn, options: Object = null): Void {
            if (_wrapFilters == null) {
                _wrapFilters = []
            }
            _wrapFilters.append([fn, options])
        }

        private function runFilters(filters: Array): Void {
            if (!filters) {
                return
            }
            for each (filter in filters) {
                let fn = filter[0]
                let options = filter[1]
                if (options) {
                    only = options.only
                    if (only) {
                        if (only is String && actionName != only) {
                            continue
                        }
                        if (only is Array && !only.contains(actionName)) {
                            continue
                        }
                    } 
                    except = options.except
                    if (except) {
                        if (except is String && actionName == except) {
                            continue
                        }
                        if (except is Array && except.contains(actionName)) {
                            continue
                        }
                    }
                }
                fn.call(this)
            }
        }

        /**
         *  Invoke the named action. Internal use only. Called from ejsWeb.c.
         *  @param act Action name to invoke
         */
        function doAction(act: String): Void {
            global."ejs.web"::["controller"] = this
            if (act == "") {
                act = "index"
            }
            actionName = act

            use namespace action

            if (this[actionName] == undefined) {
                originalActionName = actionName
                actionName = "missing"
            }

            let lastFlash = null
            if (session) {
                flash = session["__flash__"]
            }
            if (flash == "" || flash == undefined) {
                flash = {}
            } else {
                if (session) {
                    session["__flash__"] = undefined
                }
                lastFlash = flash.clone()
            }

            runFilters(_beforeFilters)

            if (!redirected) {
                try {
                    this[actionName]()
                    if (!rendered) {
                        renderView()
                    }
                } catch (e) {
                    reportError(Http.ServerError, "Error in action: " + actionName, e)
                    rendered = true
                    return
                }
                runFilters(_afterFilters)
            }
            if (lastFlash) {
                for (item in flash) {
                    for each (old in lastFlash) {
                        if (hashcode(flash[item]) == hashcode(old)) {
                            delete flash[item]
                        }
                    }
                }
            }
            if (flash && flash.length > 0) {
                if (session) {
                    session["__flash__"] = flash
                }
            }
            // Memory.stats()
        }

        /**
         *  Send an error response back to the client. This calls discard the output.
         *  @param code Http status code
         *  @param msg Message to display
         */
        native function sendError(code: Number, msg: String): Void

        /** @hide */
        function renderError(code: Number, msg: String = ""): Void {
            sendError(code, msg)
            rendered = true
        }

        /**
         *  Transform a string to be safe for output into an HTML web page. It does this by changing the
         *  ">", "<" and '"' characters into their ampersand HTML equivalents.
         *  @param s input string
         *  @returns a transformed HTML escaped string
         */
        function escapeHtml(s: String): String
            s.replace(/&/g,'&amp;').replace(/\>/g,'&gt;').replace(/</g,'&lt;').replace(/"/g,'&quot;')

        /** 
         *  HTML encode the arguments
         *  @param args Variable arguments that will be converted to safe html
         *  @return A string containing the encoded arguments catenated together
         */
        function html(...args): String {
            result = ""
            for each (let s: String in args) {
                result += escapeHtml(s)
            }
            return result
        }

        /**
         *  Send a positive notification to the user. This is just a convenience instead of setting flash["inform"]
         *  @param msg Message to display
         */
        function inform(msg: String): Void
            flash["inform"] = msg

        /**
         *  Send an error notification to the user. This is just a convenience instead of setting flash["error"]
         *  @param msg Message to display
         */
        function error(msg: String): Void
            flash["error"] = msg

        /**
         *  Control whether the HTTP connection is kept alive after this request
         *  @parm on Set to true to enable keep alive.
         */
        native function keepAlive(on: Boolean): Void

        /**
         *  Load a view. If path is not supplied, use the default view for the current action.
         *  @param viewName Name of the view to load. It may be a fully qualified path name or a filename relative to the 
         *      views directory for the current action, but without the 'ejs' extension.
         */
        native function loadView(path: String = null): Void

        /**
         *  Make a URL suitable for invoking actions. This routine will construct a URL Based on a supplied action name, 
         *  model id and options that may contain an optional controller name. This is a convenience routine remove from 
         *  applications the burden of building URLs that correctly use action and controller names.
         *  @params action The action name to invoke in the URL. If the name starts with "." or "/", it is assumed to be 
         *      a controller name and it is used by itself.
         *  @params id The model record ID to select via the URL. Defaults to null.
         *  @params options The options string
         *  @params query Query
         *  @return A string URL.
         *  @options url An override url to use. All other args are ignored.
         *  @options query Query string to append to the URL. Overridden by the query arg.
         *  @options controller The name of the controller to use in the URL.
         */
        function makeUrl(action: String, id: String = null, options: Object = {}, query: Object = null): String {
            if (options["url"]) {
                return options.url
            }
            let url: String
            if (action.contains("http://") || action.startsWith("/") || action.startsWith(".") || action.startsWith("#")) {
                url = action
            } else {
                url = appUrl.trim("/")
                let cname = (options && options["controller"]) || controllerName
/*
                if (url != "") {
                    url = "/" + url
                }
                url += "/" + cname + "/" + action
*/
                if (url != "") {
                    url += "/" + cname + "/" + action
                } else {
                    url = cname + "/" + action
                }
            }
            if (id && id != "undefined" && id != null) {
                id = "id=" + id
            } else {
                id = null
            }
            query ||= options.query
            if (id || query) {
                if (url.indexOf('?') < 0) {
                    url += "?"
                } else {
                    url += "&"
                }
                if (id) {
                    url += id
                    if (query) {
                        url += "&" + query
                    }
                } else if (query) {
                    url += query
                }
            }
            return url
        }

        /**
         *  Redirect the client to a new URL. This call redirects the client's browser to a new location specified 
         *  by the @url.  Optionally, a redirection code may be provided. Normally this code is set to be the HTTP 
         *  code 302 which means a temporary redirect. A 301, permanent redirect code may be explicitly set.
         *  @param url Url to redirect the client to
         *  @param code Optional HTTP redirection code
         */
        native function redirectUrl(url: String, code: Number = 302): Void

        /**
         *  Redirect to the given action
         *  @param action Action to redirect to
         *  @param id Request ID
            @param options Call options
         *  @option id controller
         */
        function redirect(action: String, id: String = null, options: Object = {}): Void {
            redirectUrl(makeUrl(action, id, options))
            redirected = true
        }

        /**
         *  Render the raw arguments back to the client. The args are converted to strings.
         */
        function render(...args): Void { 
            rendered = true
            write(args)
        }

        /**
         *  Render a file's contents. 
         */
        function renderFile(filename: String): Void { 
            rendered = true
            let file: File = new File(filename)
            try {
                file.open()
                while (data = file.read(4096)) {
                    writeRaw(data)
                }
                file.close()
            } catch (e: Error) {
                reportError(Http.ServerError, "Can't read file: " + filename, e)
            }
        }

        # FUTURE
        function renderPartial(): void { }

        /**
         *  Render raw data
         */
        function renderRaw(...args): Void {
            rendered = true
            writeRaw(args)
        }

        # FUTURE
        function renderXml(): Void {}

        # FUTURE
        function renderJSON(): Void {}

        /**
         *  Render a view template
         */
        function renderView(viewName: String = null): Void {
            if (rendered) {
                throw new Error("renderView invoked but render has already been called")
                return
            }
            rendered = true

            if (viewName == null) {
                viewName = actionName
            }
            
            try {
                let name = Reflect(this).name
                let viewClass: String = name.trim("Controller") + "_" + viewName + "View"
                if (global[viewClass] == undefined) {
                    loadView(viewName)
                }
                view = new global[viewClass](this)

            } catch (e: Error) {
                if (e.code == undefined) {
                    e.code = Http.ServerError
                }
                if (extension(request.url) == "ejs") {
                    reportError(e.code, "Can't load page: " + request.url, e)
                } else {
                    reportError(e.code, "Can't load view: " + viewName + ".ejs" + " for " + request.url, e)
                }
                return
            }

            try {
                for (let n: String in this) {
                    view.public::[n] = this[n]
                }
                view.render()
            } catch (e: Error) {
                reportError(Http.ServerError, 'Error rendering: "' + viewName + '.ejs".', e)
            } catch (msg) {
                reportError(Http.ServerError, 'Error rendering: "' + viewName + '.ejs". ' + msg)
            }
        }

        /**
         *  Report errors back to the client
         *  @param msg Message to send to the client
         *  @param e Optional Error exception object to include in the message
         */
        private function reportError(code: Number, msg: String, e: Object = null): Void {
            if (code <= 0) {
                code = Http.ServerError
            }
            if (e) {
                e = e.toString().replace(/.*Error Exception: /, "")
            }
            if (host.logErrors) {
                if (e) {
                    msg += "\r\n" + e
                }
            } else {
                msg = "<h1>Ejscript error for \"" + request.url + "\"</h1>\r\n<h2>" + msg + "</h2>\r\n"
                if (e) {
                    msg += "<pre>" + escapeHtml(e) + "</pre>\r\n"
                }
                msg += '<p>To prevent errors being displayed in the "browser, ' + 
                    'use <b>"EjsErrors log"</b> in the config file.</p>\r\n'
            }
            sendError(code, msg)
        }

        /**
         *  Define a cookie header to include in the reponse. Path, domain and lifetime can be set to null for default
         *  values.
         *  @param name Cookie name
         *  @param value Cookie value
         *  @param path URI path to which the cookie applies
         *  @param domain Domain in which the cookie applies. Must have 2-3 dots.
         *  @param lifetime Duration for the cookie to persist in seconds
         *  @param secure Set to true if the cookie only applies for SSL based connections
         */
        native function setCookie(name: String, value: String, path: String = null, domain: String = null, 
                lifetime: Number = 0, secure: Boolean = false): Void

        /**
         *  of the format "keyword: value". If a header has already been defined and \a allowMultiple is false, 
         *  the header will be overwritten. If \a allowMultiple is true, the new header will be appended to the 
         *  response headers and the existing header will also be output. NOTE: case does not matter in the header keyword.
         *  @param key Header key string
         *  @param value Header value 
         *  @param allowMultiple If false, overwrite existing headers with the same keyword. If true, all headers are output.
         */
        native function setHeader(key: String, value: String, allowMultiple: Boolean = false): Void

        /**
         *  Set the HTTP response status code
         *  @param code HTTP status code to define
         */
        native function setHttpCode(code: Number): Void

        /**
         *  Set the response body mime type
         *  @param format Mime type for the response. For example "text/plain".
         */
        native function setMimeType(format: String): Void

        /**
         *  Transform an escaped string into its original contents. This reverses the transformation done by $escapeHtml.
         *  It does this by changing &quot, &gt, &lt back into ", < and >.
         *  @param s input string
         *  @returns a transformed string
         */
        function unescapeHtml(s: String): String
            s.replace(/&amp/g,'&;').replace(/&gt/g,'>').replace(/&lt/g,'<').replace(/&quot/g,'"')

        /**
         *  Send a warning message back to the client for display in the flash area. This is just a convenience instead of
         *  setting flash["warn"]
         *  @param msg Message to display
         */
        function warn(msg: String): Void
            flash["warning"] = msg

        /**
         *  Write text to the client. This call writes the arguments back to the client's browser. The arguments 
         *  are converted to strings before writing back to the client. Text written using write, will be buffered 
         *  up to a configurable maximum. This allows text to be written prior to setting HTTP headers with setHeader.
         *  @param args Text or objects to write to the client
         */
        native function write(...args): Void

        /**
         *  Send text back to the client which must first be HTML escaped
         *  @param args Objects to emit
         */
        function writeHtml(...args): Void
            write(html(args))

        /** @hide */
        native function writeRaw(...args): Void

        /**
         *  Missing action method. This method will be called if the requested action routine does not exist.
         */
        action function missing(): Void {
            msg = "<h1>Missing Action</h1>" +
                "<h3>Action: \"" + originalActionName + "\" could not be found for controller \"" + 
                controllerName + "\".</h3>"
            sendError(500, msg)
            rendered = true
        }
    }

    /** 
     *  Controller for use by stand-alone web pages
     *  @stability prototype
     *  @hide 
     */
    class _SoloController extends Controller {
    }
}


/*
 *  @copy   default
 *  
 *  Copyright (c) Embedthis Software LLC, 2003-2010. All Rights Reserved.
 *  Copyright (c) Michael O'Brien, 1993-2010. All Rights Reserved.
 *  
 *  This software is distributed under commercial and open source licenses.
 *  You may use the GPL open source license described below or you may acquire 
 *  a commercial license from Embedthis Software. You agree to be fully bound 
 *  by the terms of either license. Consult the LICENSE.TXT distributed with 
 *  this software for full details.
 *  
 *  This software is open source; you can redistribute it and/or modify it 
 *  under the terms of the GNU General Public License as published by the 
 *  Free Software Foundation; either version 2 of the License, or (at your 
 *  option) any later version. See the GNU General Public License for more 
 *  details at: http://www.embedthis.com/downloads/gplLicense.html
 *  
 *  This program is distributed WITHOUT ANY WARRANTY; without even the 
 *  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
 *  
 *  This GPL license does NOT permit incorporating this software into 
 *  proprietary programs. If you are unable to comply with the GPL, you must
 *  acquire a commercial license to use this software. Commercial licenses 
 *  for this software and support services are available from Embedthis 
 *  Software at http://www.embedthis.com 
 *
 *  @end
 */
/************************************************************************/
/*
 *  End of file "../src/es/web/Controller.es"
 */
/************************************************************************/



/************************************************************************/
/*
 *  Start of file "../src/es/web/Cookie.es"
 */
/************************************************************************/

/**
 *  Cookie.es -- Cookie class
 */

module ejs.web {

    /**
     *  Cookie class to store parsed cookie strings. The MVC web framework creates Cookie class instances when
     *  the client provides Http cookie headers.
     *  @spec ejs
     *  @stability prototype
     */
    class Cookie {

        use default namespace public

        /**
         *  Name of the cookie
         */
        var name: String

        /**
         *  Value of the cookie
         */
        var value: String

        /**
         *  Domain in which the cookie applies
         */
        var domain: String

        /**
         *  URI path in which the cookie applies
         */
        var path: String
    }
}


/*
 *  @copy   default
 *  
 *  Copyright (c) Embedthis Software LLC, 2003-2010. All Rights Reserved.
 *  Copyright (c) Michael O'Brien, 1993-2010. All Rights Reserved.
 *  
 *  This software is distributed under commercial and open source licenses.
 *  You may use the GPL open source license described below or you may acquire 
 *  a commercial license from Embedthis Software. You agree to be fully bound 
 *  by the terms of either license. Consult the LICENSE.TXT distributed with 
 *  this software for full details.
 *  
 *  This software is open source; you can redistribute it and/or modify it 
 *  under the terms of the GNU General Public License as published by the 
 *  Free Software Foundation; either version 2 of the License, or (at your 
 *  option) any later version. See the GNU General Public License for more 
 *  details at: http://www.embedthis.com/downloads/gplLicense.html
 *  
 *  This program is distributed WITHOUT ANY WARRANTY; without even the 
 *  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
 *  
 *  This GPL license does NOT permit incorporating this software into 
 *  proprietary programs. If you are unable to comply with the GPL, you must
 *  acquire a commercial license to use this software. Commercial licenses 
 *  for this software and support services are available from Embedthis 
 *  Software at http://www.embedthis.com 
 *
 *  @end
 */
/************************************************************************/
/*
 *  End of file "../src/es/web/Cookie.es"
 */
/************************************************************************/



/************************************************************************/
/*
 *  Start of file "../src/es/web/Host.es"
 */
/************************************************************************/

/**
 *  Host.es - Host class for the Ejscript web framework
 */

module ejs.web {

    /**
     *  Web server host information. The server array stores information that typically does not vary from 
     *  request to request. For a given virtual server, these data items will be constant across all requests.
     *  @spec ejs
     *  @stability prototype
     */
    final class Host {

        use default namespace public

        /**
         *  Home directory for the web documents
         */
        native var documentRoot: String

        /**
         *  Fully qualified name of the server. Should be of the form (http://name[:port])
         */
        native var name: String

        /**
         *  Host protocol (http or https)
         */
        native var protocol: String

        /**
         *  Set if the host is a virtual host
         */ 
        native var isVirtualHost: Boolean

        /**
         *  Set if the host is a named virtual host
         */
        native var isNamedVirtualHost: Boolean

        /**
         *  Server software description
         */
        native var software: String

        /**
         *  Log errors to the application log
         */
        native var logErrors: Boolean
    }
}
/************************************************************************/
/*
 *  End of file "../src/es/web/Host.es"
 */
/************************************************************************/



/************************************************************************/
/*
 *  Start of file "../src/es/web/Request.es"
 */
/************************************************************************/

/**
 *  Request.es - Request class for the Ejscript web framework
 */

module ejs.web {

    /**
     *  HTTP request information. The request objects stores parsed information for incoming HTTP requests.
     *  @spec ejs
     *  @stability prototype
     */
    final class Request {

        use default namespace public

        /**
         *  Accept header
         */
        native var accept: String

        /**
         *  AcceptCharset header
         */
        native var acceptCharset: String

        /**
         *  AcceptEncoding header
         */
        native var acceptEncoding: String

        /**
         *  Authentication access control list
         */
        native var authAcl: String

        /**
         *  Authentication group
         */
        native var authGroup: String

        /**
         *  Authentication method if authorization is being used (basic or digest)
         */
        native var authType: String

        /**
         *  Authentication user name
         */
        native var authUser: String

        /**
         *  Request content body. NOTE: This property will be replaced with a stream based input scheme in a future
         *  release.
         *  @prototype.
         */
        native var body: String

        /**
         *  Connection header
         */
        native var connection: String

        /**
         *  Posted content length (header: Content-Length)
         */
        native var contentLength: Number

        /**
         *  Stores Client cookie state information. The cookies object will be created automatically if the Client supplied 
         *  cookies with the current request. Cookies are used to specify the session state. If sessions are being used, 
         *  a session cookie will be sent to and from the browser with each request. The elements are user defined.
         */
        native var cookies: Object
            
        /**
         *  Extension portion of the URL after aliasing to a filename.
         */
        native var extension: String

        /**
         *  Files uploaded as part of the request. For each uploaded file, an instance of UploadFile is created in files. 
         *  The property name of the instance is given by the file upload HTML input element ID in the request page form. 
         *  Set to null if no files have been uploaded.
         */
        native var files: Object

        /**
         *  Store the request headers. The request array stores all the HTTP request headers that were supplied by 
         *  the client in the current request. 
         */
        native var headers: Object

        /**
         *  The host name header
         */
        native var hostName: String

        /**
         *  Request method: DELETE, GET, POST, PUT, OPTIONS, TRACE
         */
        native var method: String

        /**
         *  Content mime type (header: Content-Type)
         */
        native var mimeType: String

        /**
         *  The portion of the path after the script name if extra path processing is being used. See the ExtraPath 
         *  directive.
         */
        native var pathInfo: String

        /**
         *  The physical path corresponding to PATH_INFO.
         */
        native var pathTranslated

        /**
         *  Pragma header
         */
        native var pragma: String

        /**
         *  Decoded Query string (URL query string)
         */
        native var query: String

        /**
         *  Raw request URI before decoding.
         */
        native var originalUri: String

        /**
         *  Name of the referring URL
         */
        native var referrer: String

        /**
         *  The IP address of the Client issuing the request.
         */
        native var remoteAddress: String

        /**
         *  The host address of the Client issuing the request
         *  This is deprecated due to the security issues with doing reverse DNS lookups. Use remoteAddress instead.
         *  @hide
         */
        native var remoteHost: String

        /**
         *  Current session ID. Index into the $sessions object
         */
        native var sessionID: String

        /**
         *  The decoded request URL portion after stripping the scheme, host, extra path, query and fragments
         */
        native var url: String

        /**
         *  Name of the Client browser software set in the HTTP_USER_AGENT header
         */
        native var userAgent: String
    }
}
/************************************************************************/
/*
 *  End of file "../src/es/web/Request.es"
 */
/************************************************************************/



/************************************************************************/
/*
 *  Start of file "../src/es/web/Response.es"
 */
/************************************************************************/

/**
 *  Response.es - Response object for the Ejscript web framework.
 */

module ejs.web {

    /**
     *  HTTP response class. The Http response object stores information about HTTP responses.
     *  @spec ejs
     *  @stability prototype
     */
    final class Response {

        use default namespace public

        /**
         *  HTTP response code
         */
        native var code: Number

        /**
         *  Response content length. 
         */
        # FUTURE
        native var contentLength: Number

        /**
         *  Cookies to send to the client. These are currently stored in headers[]
         */
        # FUTURE
        native var cookies: Object

        /**
         *  Unique response tag - not generated  yet
         */
        # FUTURE
        native var etag: String

        /**
         *  Filename for the Script name
         */
        native var filename: String

        /**
         *  Reponse headers
         */
        native var headers: Array

        /**
         *  Response content mime type
         */
        native var mimeType: String
    }
}
/************************************************************************/
/*
 *  End of file "../src/es/web/Response.es"
 */
/************************************************************************/



/************************************************************************/
/*
 *  Start of file "../src/es/web/Session.es"
 */
/************************************************************************/

/**
 *  Session.es -- Session state management
 */

module ejs.web {

    /**
     *  Hash of all sessions. Held by the master interpreter.
     *  @spec ejs
     *  @stability prototype
     */
    var sessions = {}

    /**
     *  Session state storage class. 
     *  @spec ejs
     *  @stability prototype
     *  @hide
     */
    dynamic class Session { }
}


/*
 *  @copy   default
 *  
 *  Copyright (c) Embedthis Software LLC, 2003-2010. All Rights Reserved.
 *  Copyright (c) Michael O'Brien, 1993-2010. All Rights Reserved.
 *  
 *  This software is distributed under commercial and open source licenses.
 *  You may use the GPL open source license described below or you may acquire 
 *  a commercial license from Embedthis Software. You agree to be fully bound 
 *  by the terms of either license. Consult the LICENSE.TXT distributed with 
 *  this software for full details.
 *  
 *  This software is open source; you can redistribute it and/or modify it 
 *  under the terms of the GNU General Public License as published by the 
 *  Free Software Foundation; either version 2 of the License, or (at your 
 *  option) any later version. See the GNU General Public License for more 
 *  details at: http://www.embedthis.com/downloads/gplLicense.html
 *  
 *  This program is distributed WITHOUT ANY WARRANTY; without even the 
 *  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
 *  
 *  This GPL license does NOT permit incorporating this software into 
 *  proprietary programs. If you are unable to comply with the GPL, you must
 *  acquire a commercial license to use this software. Commercial licenses 
 *  for this software and support services are available from Embedthis 
 *  Software at http://www.embedthis.com 
 *
 *  @end
 */
/************************************************************************/
/*
 *  End of file "../src/es/web/Session.es"
 */
/************************************************************************/



/************************************************************************/
/*
 *  Start of file "../src/es/web/UploadFile.es"
 */
/************************************************************************/

/**
 *  uploadedFile.es - 
 */

module ejs.web {

    /**
     *  Upload file class. Users do not instantiate this class. Rather, the web framework will allocate and store
     *  instances in the Request.files hash.
     *  @spec ejs
     *  @stability evolving
     */
    class UploadFile {

        use default namespace public

        /**
         *  Name of the uploaded file given by the client
         */
        native var clientFilename: String

        /**
         *  Mime type of the encoded data
         */
        native var contentType: String

        /**
         *  Name of the uploaded file. This is a temporary file in the upload directory.
         */
        native var filename: String

        /**
         *  HTML input ID for the upload file element
         */
        var name: String
 
        /**
         *  Size of the uploaded file in bytes
         */
        native var size: Number
    }
}

/************************************************************************/
/*
 *  End of file "../src/es/web/UploadFile.es"
 */
/************************************************************************/



/************************************************************************/
/*
 *  Start of file "../src/es/web/View.es"
 */
/************************************************************************/

/**
    View.es -- View class as part of the Ejscript MVC framework
 */
module ejs.web {

    require ejs.db

    /**
        Base class for web framework views. This class provides the core functionality for all Ejscript view web pages.
        Ejscript web pages are compiled to create a new View class which extends the View base class. In addition to
        the properties defined by this class, user view classes will inherit at runtime all public properites of the
        current controller object. View objects are not instantiated manually by users but are created internally 
        by the web framework.
        @spec ejs
        @stability prototype
     */
    dynamic class View {
        /*
            Define properties and functions are (by default) in the ejs.web namespace rather than public to avoid clashes.
         */
        use default namespace "ejs.web"

        /**
            Current controller
         */
        var controller: Controller

        /*
            Current model being used inside a form
         */
        private var currentModel: Record

        /*
            Configuration from the applications config *.ecf
         */
        private var config: Object

        /*
            Id generator
         */
        private var nextId: Number = 0

        /** @hide */
        function getNextId(): Number {
            return "id_" + nextId++
        }

        /**
            Constructor method to initialize a new View
            @param controller Controller to manage this view
         */
        function View(controller: Controller) {
            this.controller = controller
            this.config = controller.config
            view = this
        }

        /**
            Process and emit a view to the client. Overridden by the views invoked by controllers.
         */
        public function render(): Void {}

        /************************************************ View Helpers ****************************************************/

        /**
            Render an asynchronous (ajax) form.
            @param action Action to invoke when the form is submitted. Defaults to "create" or "update" depending on 
                whether the field has been previously saved.
            @param record Model record to edit
            @param options Optional extra options. See $getOptions for a list of the standard options.
            @option url String Use a URL rather than action and controller for the target url.
         */
        function aform(action: String, record: Object = null, options: Object = {}): Void {
            if (record == null) {
                record = new LocalModel
            }
            currentModel = record
            formErrors(record)
            options = setOptions("aform", options)
            if (options.method == null) {
                options.method = "POST"
            }
            if (action == null) {
                action = "update"
            }
            let connector = getConnector("aform", options)
            options.url = makeUrl(action, record.id, options)
            connector.aform(record, options.url, options)
        }

        /** 
            Emit an asynchronous (ajax) link to an action. The URL is constructed from the given action and the 
                current controller. The controller may be overridden by setting the controller option.
            @param text Link text to display
            @param action Action to invoke when the link is clicked
            @param options Optional extra options. See $getOptions for a list of the standard options.
            @option controller String Name of the target controller for the given action
            @option url String Use a URL rather than action and controller for the target url.
         */
        function alink(text: String, action: String = null, options: Object = {}): Void {
            if (action == null) {
                action = text.split(" ")[0].toLower()
            }
            options = setOptions("alink", options)
            if (options.method == null) {
                options.method = "POST"
            }
            let connector = getConnector("alink", options)
            options.url = makeUrl(action, options.id, options)
            connector.alink(text, options.url, options)
        }

        /**
            Render a form button. This creates a button suitable for use inside an input form. When the button is clicked,
            the input form will be submitted.
            @param value Text to display in the button.
            @param name Form name of button.
            @param options Optional extra options. See $getOptions for a list of the standard options.
            Examples:
                button("Click Me", "OK")
         */
        function button(value: String, buttonName: String = null, options: Object = {}): Void {
            options = setOptions("button", options)
            if (buttonName == null) {
                buttonName = value.toLower()
            }
            let connector = getConnector("button", options)
            connector.button(value, buttonName, options)
        }

        /**
            Render a link button. This creates a button suitable for use outside an input form. When the button is clicked,
            the associated URL will be invoked.
            @param text Text to display in the button.
            @param action Target action to invoke when the button is clicked.
            @param url Override target URL to use instead of action.
            @param options Optional extra options. See $getOptions for a list of the standard options.
         */
        function buttonLink(text: String, action: String, options: Object = {}): Void {
            options = setOptions("buttonLink", options)
            let connector = getConnector("buttonLink", options)
            connector.buttonLink(text, makeUrl(action, options.id, options), options)
        }

        /**
            Render a chart. The chart control can display static or dynamic tabular data. The client chart control manages
            sorting by column, dynamic data refreshes, pagination and clicking on rows.
            @param initialData Optional initial data for the control. The data option may be used with the refresh option to 
                dynamically refresh the data.
            @param options Object Optional extra options. See also $getOptions for a list of the standard options.
            @option columns Object hash of column entries. Each column entry is in-turn an object hash of options. If unset, 
                all columns are displayed using defaults.
            @option kind String Type of chart. Select from: piechart, table, linechart, annotatedtimeline, guage, map, 
                motionchart, areachart, intensitymap, imageareachart, barchart, imagebarchart, bioheatmap, columnchart, 
                linechart, imagelinechart, imagepiechart, scatterchart (and more)
            @option onClick String Action or URL to invoke when a chart element  is clicked.
            @example
                <% chart(null, { data: "getData", refresh: 2" }) %>
                <% chart(data, { onClick: "action" }) %>
         */
        function chart(initialData: Array, options: Object = {}): Void {
            let connector = getConnector("chart", options)
            connector.chart(initialData, options)
        }

        /**
            Render an input checkbox. This creates a checkbox suitable for use within an input form. 
            @param field Name of the field to display. This is used to create a HTML "name" and "id" attribute for the 
                input element. If used inside a model form, it is the field name in the model containing the checkbox
                value to display. If used without a model, the value to display should be passed via options.value. 
            @param choice Value to submit if checked. Defaults to "true"
            @param options Optional extra options. See $getOptions for a list of the standard options.
         */
        function checkbox(field: String, choice: String = "true", options: Object = {}): Void {
            options = setOptions(field, options)
            let value = getValue(currentModel, field, options)
            let connector = getConnector("checkbox", options)
            connector.checkbox(options.fieldName, value, choice, options)
        }

        /**
            End an input form. This closes an input form initiated by calling the $form method.
         */
        function endform(): Void {
            let connector = getConnector("endform", null)
            connector.endform()
            currentModel = undefined
        }

        /**
            Render a form.
            @param action Action to invoke when the form is submitted. Defaults to "create" or "update" depending on 
                whether the field has been previously saved.
            @param record Model record to edit
            @param options Optional extra options. See $getOptions for a list of the standard options.
            @option url String Use a URL rather than action and controller for the target url.
         */
        function form(action: String, record: Object = null, options: Object = {}): Void {
            /*
            if (record == null) {
                record = new LocalModel
            }
            */
            currentModel = record
            formErrors(record)
            options = setOptions("form", options)
            if (options.method == null) {
                options.method = "POST"
            }
            if (action == null) {
                action = "update"
            }
            let connector = getConnector("form", options)
            options.url = makeUrl(action, (record) ? record.id : null, options)
            connector.form(record, options.url, options)
        }

        /**
            Render an image control
            @param src Optional initial source name for the image. The data option may be used with the refresh option to 
                dynamically refresh the data.
            @param options Optional extra options. See $getOptions for a list of the standard options.
            @examples
                <% image("myPic.gif") %>
                <% image("myPic.gif", { data: "getData", refresh: 2, style: "myStyle" }) %>
         */
        function image(src: String, options: Object = {}): Void {
            let connector = getConnector("image", options)
            connector.image(src, options)
        }

        /**
            Render a clickable image. This creates an clickable image suitable for use outside an input form. 
            When the image is clicked, the associated URL will be invoked.
            @param image Optional initial source name for the image. The data option may be used with the refresh option to 
                dynamically refresh the data.
            @param action Target action to invoke when the image is clicked.
            @param url Override target URL to use instead of action.
            @param options Optional extra options. See $getOptions for a list of the standard options.
         */
        function imageLink(image: String, action: String, options: Object = {}): Void {
            options = setOptions("imageLink", options)
            let connector = getConnector("imageLink", options)
            connector.imageLink(text, makeUrl(action, options.id, options), options)
        }

        /**
            Render an input field as part of a form. This is a smart input control that will call the appropriate
                input control based on the model field data type.
            @param field Model field name containing the text data for the control.
            @param options Optional extra options. See $getOptions for a list of the standard options.
            @examples
                <% input(modelFieldName) %>
                <% input(null, { options }) %>
         */
        function input(field: String, options: Object = {}): Void {
            datatype = currentModel.getColumnType(field)
            switch (datatype) {
            case "binary":
            case "date":
            case "datetime":
            case "decimal":
            case "float":
            case "integer":
            case "number":
            case "string":
            case "time":
            case "timestamp":
                text(field, options)
                break

            case "text":
                textarea(field, options)
                break

            case "boolean":
                checkbox(field, "true", options)
                break

            default:
                throw "input control: Unknown field type: " + datatype + " for field " + field
            }
        }

        /**
            Render a text label field. This renders an output-only text field. Use text() for input fields.
            @param text Optional initial data for the control. The data option may be used with the refresh option to 
                dynamically refresh the data.
            @param options Optional extra options. See $getOptions for a list of the standard options.
            @examples
                <% label("Hello World") %>
                <% label(null, { data: "getData", refresh: 2, style: "myStyle" }) %>
         */
        function label(text: String, options: Object = {}): Void {
            options = setOptions("label", options)
            let connector = getConnector("label", options)
            connector.label(text, options)
        }

        /** 
            Emit a link to an action. The URL is constructed from the given action and the current controller. The controller
            may be overridden by setting the controller option.
            @param text Link text to display
            @param action Action to invoke when the link is clicked
            @param options Optional extra options. See $getOptions for a list of the standard options.
            @option controller String Name of the target controller for the given action
            @option url String Use a URL rather than action and controller for the target url.
         */
        function link(text: String, action: String = null, options: Object = {}): Void {
            if (action == null) {
                action = text.split(" ")[0].toLower()
            }
            options = setOptions("link", options)
            let connector = getConnector("link", options)
            connector.link(text, makeUrl(action, options.id, options), options)
        }

        /** 
            Emit an application relative link. If invoking an action, it is safer to use \a action.
            @param text Link text to display
            @param url Action or URL to invoke when the link is clicked
            @param options Optional extra options. See $getOptions for a list of the standard options.
         */
        function extlink(text: String, url: String, options: Object = {}): Void {
            let connector = getConnector("extlink", options)
            connector.extlink(text, controller.appUrl + url, options)
        }

        /**
            Emit a selection list. 
            @param field Name of the field to display. This is used to create a HTML "name" and "id" attribute for the 
                input element. If used inside a model form, it is the field name in the model containing the list item to
                select. If used without a model, the value to select should be passed via options.value. 
            @param choices Choices to select from. This can be an array list where each element is displayed and the value 
                returned is an element index (origin zero). It can also be an array of array tuples where the first 
                tuple entry is the value to display and the second is the value to send to the app. Or it can be an 
                array of objects such as those returned from a table lookup. If choices is null, the $field value is 
                used to construct a model class name to use to return a data grid containing an array of row objects. 
                The first non-id field is used as the value to display.
            @params options Extra options
            Examples:
                list("stockId", Stock.stockList) 
                list("low", ["low", "med", "high"])
                list("low", [["low", "3"], ["med", "5"], ["high", "9"]])
                list("low", [{low: 3{, {med: 5}, {high: 9}])
                list("Stock Type")                          Will invoke StockType.findAll() to do a table lookup
         */
        function list(field: String, choices: Object = null, options: Object = {}): Void {
            options = setOptions(field, options)
            if (choices == null) {
                modelTypeName = field.replace(/\s/, "").toPascal()
                modelTypeName = modelTypeName.replace(/Id$/, "")
                if (global[modelTypeName] == undefined) {
                    throw new Error("Can't find model to create list data: " + modelTypeName)
                }
                choices = global[modelTypeName].findAll()
            }
            let value = getValue(currentModel, field, options)
            let connector = getConnector("list", options)
            connector.list(options.fieldName, choices, value, options)
        }

        /**
            Emit a mail link
            @param nameText Recipient name to display
            @param address Mail recipient address
            @param options Optional extra options. See $getOptions for a list of the standard options.
         */
        function mail(nameText: String, address: String, options: Object = {}): Void  {
            let connector = getConnector("mail", options)
            connector.mail(nameText, address, options)
        }

        /** 
            Emit a progress bar. 
            @param initialData Optional initial data for the control. The data option may be used with the refresh option 
                to dynamically refresh the data. Progress data is a simple Number in the range 0 to 100 and represents 
                a percentage completion value.
            @param options Optional extra options. See $getOptions for a list of the standard options.
            @example
                <% progress(null, { data: "getData", refresh: 2" }) %>
            @hide
         */
        function progress(initialData: Object, options: Object = {}): Void {
            let connector = getConnector("progress", options)
            connector.progress(initialData, options)
        }

        /** 
            Emit a radio autton. The URL is constructed from the given action and the current controller. The controller
                may be overridden by setting the controller option.
            @param field Name of the field to display. This is used to create a HTML "name" and "id" attribute for the 
                input element. If used inside a model form, it is the field name in the model containing the radio data to
                display. If used without a model, the value to display should be passed via options.value. 
            @param choices Array or object containing the option values. If array, each element is a radio option. If an 
                object hash, then they property name is the radio text to display and the property value is what is returned.
            @param action Action to invoke when the button is clicked or invoked
            @param options Optional extra options. See $getOptions for a list of the standard options.
            @option controller String Name of the target controller for the given action
            @option value String Name of the option to select by default
            @example
                radio("priority", ["low", "med", "high"])
                radio("priority", {low: 0, med: 1, high: 2})
                radio(priority, Message.priorities)
         */
        function radio(field: String, choices: Object, options: Object = {}): Void {
            options = setOptions(field, options)
            let value = getValue(currentModel, field, options)
            let connector = getConnector("radio", options)
            connector.radio(options.fieldName, value, choices, options)
        }

        /** 
            Emit a script link.
            @param url URL for the script to load
            @param options Optional extra options. See $getOptions for a list of the standard options.
         */
        function script(url: Object, options: Object = {}): Void {
            let connector = getConnector("script", options)
            if (url is Array) {
                for each (u in url) {
                    connector.script(controller.appUrl + "/" + u, options)
                }
            } else {
                connector.script(controller.appUrl + "/" + url, options)
            }
        }

        /** 
            Emit a status control area. 
            @param initialData Optional initial data for the control. The data option may be used with the refresh option to 
                dynamically refresh the data. Status data is a simple String. Status messages may be updated by calling the
                \a statusMessage function.
            @param options Optional extra options. See $getOptions for a list of the standard options.
            @example
                <% status("Initial Status Message", { data: "getData", refresh: 2" }) %>
            @hide
         */
        function status(initialData: Object, options: Object = {}): Void {
            let connector = getConnector("status", options)
            connector.status(initialData, options)
        }

        /** 
            Emit a style sheet link.
            @param url Stylesheet url or array of stylesheets
            @param options Optional extra options. See $getOptions for a list of the standard options.
         */
        function stylesheet(url: Object, options: Object = {}): Void {
            let connector = getConnector("stylesheet", options)
            if (url is Array) {
                for each (u in url) {
                    connector.stylesheet(controller.appUrl + "/" + u, options)
                }
            } else {
                connector.stylesheet(controller.appUrl + "/" + url, options)
            }
        }

        /**
            Render a table. The table control can display static or dynamic tabular data. The client table control manages
                sorting by column, dynamic data refreshes, pagination and clicking on rows. If the table supplies a URL
                or action for the data parameter, the table data is retrieved asynchronously using Ajax requests on that
                action/URL value. The action routine should call the table() control to render the data and must set the
                ajax option to true.  
            @param data Data for the control or URL/action to supply data. If data is a String, it is interpreted as a URL
                or action that will be invoked to supply HTML for the table. In this case, the refresh option defines 
                how frequently to refresh the table data. The data parameter can also be a grid of data, ie. an Array of 
                objects where each object represents the data for a row. The column names are the object property names 
                and the cell text is the object property values. The data parameter can also be a model instance.
            @param options Optional extra options. See $getOptions for a list of the standard options.
            @option ajax Set to true if the table control is being invoked as part of an Ajax data refresh.
            @option click String Action or URL to invoke an element in the table is clicked. The click arg can be is 
                a String to apply to all cells, a single-dimension array of strings for per-row URLs, and a 
                two-dimension array for per cell URLs (order is row/column).
            @option columns Object hash of column entries. Each column entry is in-turn an object hash of options. If unset, 
                all columns are displayed using defaults. Column options: align, formatter, header, render, sort, sortOrder, 
                style, width.
            @option pageSize Number Number of rows to display per page. Omit or set to <= 0 for unlimited. 
                Defaults to unlimited.
            @option pivot Boolean Pivot the table by swaping rows for columns and vice-versa
            @option query URL query string to add to click URLs. Can be a single-dimension array for per-row query 
                strings or a two-dimensional array for per cell (order is row/column).
            @option showHeader Boolean Control if column headings are displayed.
            @option showId Boolean If a columns option is not provided, the id column is normally hidden. 
                To display, set showId to be true.
            @option sort String Enable row sorting and define the column to sort by.
            @option sortOrder String Default sort order. Set to "ascending" or "descending".Defaults to ascending.
            @option style String CSS style to use for the table.
            @option styleColumns Array of styles to use for the table body columns. Can also use the style option in the
                columns option.
            @option styleBody String CSS style to use for the table body cells
            @option styleHeader String CSS style to use for the table header.
            @option styleRows Array of styles to use for the table body rows
            @option styleOddRow String CSS style to use for odd data rows in the table
            @option styleEvenRow String CSS style to use for even data rows in the table
            @option title String Table title
         *
            Column options:
            <ul>
            <li>align</li>
            <li>format</li>
            <li>formatter</li>
            <li>header</li>
            <li>render</li>
            <li>sort String Define the column to sort by and the sort order. Set to "ascending" or "descending". 
                Defaults to ascending.</li>
            <li>style</li>
            </ul>
         *
            @example
                <% table("getData", { refresh: 2, pivot: true" }) %>
                <% table(gridData, { click: "edit" }) %>
                <% table(Table.findAll()) %>
                <% table(gridData, {
                    click: "edit",
                    sort: "Product",
                    columns: {
                        product:    { header: "Product", width: "20%" }
                        date:       { format: date('%m-%d-%y) }
                    }
                 }) %>
         */
        function table(data, options: Object = {}): Void {
            options = setOptions("table", options)
            let connector = getConnector("table", options)
            if (options.pivot) {
                data = pivot(data)
            }
            connector.table(data, options)
        }

        /**
            Render a tab control. The tab control can display static or dynamic tree data.
            @param initialData Optional initial data for the control. The data option may be used with the refresh option to 
                dynamically refresh the data. Tab data is an array of objects -- one per tab. For example:
                [{"Tab One Label", "action1"}, {"Tab Two Label", "action2"}]
            @param options Optional extra options. See $getOptions for a list of the standard options.
         */
        function tabs(initialData: Array, options: Object = {}): Void {
            let connector = getConnector("tabs", options)
            connector.tabs(initialData, options)
        }

        /**
            Render a text input field as part of a form.
            @param field Name of the field to display. This is used to create a HTML "name" and "id" attribute for the 
                input element. If used inside a model form, it is the field name in the model containing the text data to
                display. If used without a model, the value to display should be passed via options.value. 
            @param options Optional extra options. See $getOptions for a list of the standard options.
            @option escape Boolean Escape the text before rendering. This converts HTML reserved tags and delimiters into
                an encoded form.
            @option style String CSS Style to use for the control
            @option visible Boolean Make the control visible. Defaults to true.
            @examples
                <% text("name") %>
         */
        function text(field: String, options: Object = {}): Void {
            options = setOptions(field, options)
            let value = getValue(currentModel, field, options)
            let connector = getConnector("text", options)
            connector.text(options.fieldName, value, options)
        }

        /**
            Render a text area
            @param field Name of the field to display. This is used to create a HTML "name" and "id" attribute for the 
                input element. If used inside a model form, it is the field name in the model containing the text data to
                display. If used without a model, the value to display should be passed via options.value. 
            @option Boolean escape Escape the text before rendering. This converts HTML reserved tags and delimiters into
                an encoded form.
            @param options Optional extra options. See $getOptions for a list of the standard options.
            @option data String URL or action to get data 
            @option numCols Number number of text columns
            @option numRows Number number of text rows
            @option style String CSS Style to use for the control
            @option visible Boolean Make the control visible. Defaults to true.
            @examples
                <% textarea("name") %>
         */
        function textarea(field: String, options: Object = {}): Void {
            options = setOptions(field, options)
            let value = getValue(currentModel, field, options)
            let connector = getConnector("textarea", options)
            connector.textarea(options.fieldName, value, options)
        }

        /**
            Render a tree control. The tree control can display static or dynamic tree data.
            @param initialData Optional initial data for the control. The data option may be used with the refresh option to 
                dynamically refresh the data. The tree data is an XML document.
            @param options Optional extra options. See $getOptions for a list of the standard options.
            @option data URL or action to get data 
            @option refresh If set, this defines the data refresh period in seconds. Only valid if the data option is defined
            @option style String CSS Style to use for the control
            @option visible Boolean Make the control visible. Defaults to true.
            @hide
         */
        function tree(initialData: XML, options: Object = {}): Void {
            let connector = getConnector("tree", options)
            connector.tree(initialData, options)
        }

        /*********************************** Wrappers for Control Methods ***********************************/
        /** 
            Emit a flash message area. 
            @param kinds Kinds of flash messages to display. May be a single string 
                ("error", "inform", "message", "warning"), an array of strings or null. If set to null (or omitted), 
                then all flash messages will be displayed.
            @param options Optional extra options. See $getOptions for a list of the standard options.
            @option retain Number. Number of seconds to retain the message. If <= 0, the message is retained until another
                message is displayed. Default is 0.
            @example
                <% flash("status") %>
                <% flash() %>
                <% flash(["error", "warning"]) %>
         */
        function flash(kinds = null, options: Object = {}): Void {
            options = setOptions("flash", options)

            let cflash = controller.flash
            if (cflash == null || cflash.length == 0) {
                return
            }

            let msgs: Object
            if (kinds is String) {
                msgs = {}
                msgs[kinds] = cflash[kinds]

            } else if (kinds is Array) {
                msgs = {}
                for each (kind in kinds) {
                    msgs[kind] = cflash[kind]
                }

            } else {
                msgs = cflash
            }

            for (kind in msgs) {
                let msg: String = msgs[kind]
                if (msg && msg != "") {
                    let connector = getConnector("flash", options)
                    options.style = "-ejs-flash -ejs-flash" + kind.toPascal()
                    connector.flash(kind, msg, options)
                }
            }
        }

        private function formErrors(model): Void {
            if (!model) {
                return
            }
            let errors = model.getErrors()
            if (errors) {
                write('<div class="-ejs-formError"><h2>The ' + Reflect(model).name.toLower() + ' has ' + 
                    errors.length + (errors.length > 1 ? ' errors' : ' error') + ' that ' +
                    ((errors.length > 1) ? 'prevent' : 'prevents') + '  it being saved.</h2>\r\n')
                write('    <p>There were problems with the following fields:</p>\r\n')
                write('    <ul>\r\n')
                for (e in errors) {
                    write('        <li>' + e.toPascal() + ' ' + errors[e] + '</li>\r\n')
                }
                write('    </ul>\r\n')
                write('</div>\r\n')
            }
        }

        /*********************************** Wrappers for Control Methods ***********************************/
        /**
            Enable session control. This enables session state management for this request and other requests from 
            the browser. If a session has not already been created, this call creates a session and sets the $sessionID 
            property in the request object. If a session already exists, this call has no effect. A cookie containing a 
            session ID is automatically created and sent to the client on the first response after creating the session. 
            If SessionAutoCreate is defined in the configuration file, then sessions will automatically be created for 
            every web request and your Ejscript web pages do not need to call createSession. Multiple requests may be 
            sent from a client's browser at the same time.  Ejscript will ensure that accesses to the sesssion object 
            are correctly serialized. 
            @param timeout Optional timeout for the session in seconds. If ommitted the default timeout is used.
         */
        function createSession(timeout: Number = 0): Void
            controller.createSession(timeout)

        /**
            Destroy a session. This call destroys the session state store that is being used for the current client. If no 
            session exists, this call has no effect.
         */
        function destroySession(): Void
            controller.destroySession()

        /** 
            HTML encode the arguments
            @param args Variable arguments that will be converted to safe html
            @return A string containing the encoded arguments catenated together
         */
        function html(...args): String
            controller.html(args)

        /**
            @duplicate ejs.web::Controller.makeUrl
         */
        function makeUrl(action: String, id: String = null, options: Object = {}): String
            controller.makeUrl(action, id, options)

        /**
            Redirect the client to a new URL. This call redirects the client's browser to a new location specified by 
            the @url. Optionally, a redirection code may be provided. Normally this code is set to be the HTTP 
            code 302 which means a temporary redirect. A 301, permanent redirect code may be explicitly set.
            @param url Url to redirect the client to
            @param code Optional HTTP redirection code
         */
        function redirectUrl(url: String, code: Number = 302): Void
            controller.redirectUrl(url, code)

        /**
            Redirect to the given action
            @param action Action URL to redirect to
            @param id Query ID value
            @param options Extra options
            @option id controller
         */
        function redirect(action: String, id: String = null, options: Object = {}): Void
            redirectUrl(makeUrl(action, id, options))

        /**
            Define a cookie header to include in the reponse
            @hide
         */
        function setCookie(name: String, value: String, lifetime: Number, path: String, secure: Boolean = false): Void
            controller.setCookie(name, value, lifetime, path, secure)

        /**
            of the format "keyword: value". If a header has already been defined and \a allowMultiple is false, 
            the header will be overwritten. If \a allowMultiple is true, the new header will be appended to the 
            response headers and the existing header will also be output. NOTE: case does not matter in the header keyword.
            @param key Header key string
            @param value Header value string
            @param allowMultiple If false, overwrite existing headers with the same keyword. If true, all headers are output.
         */
        function setHeader(key: String, value: String, allowMultiple: Boolean = false): Void
            controller.setHeader(key, value, allowMultiple)

        /**
            Set the HTTP response status code
            @param code HTTP response code to define
         */
        function setHttpCode(code: Number): Void
            controller.setHttpCode(code)

        /**
            Set the response body mime type
            @param format Mime type for the response. For example "text/plain".
         */
        function setMimeType(format: String): Void
            controller.setMimeType(format);

        /**
            @duplicate ejs.web::Controller.write
         */
        function write(...args): Void
            controller.write(args)

        /**
            Write HTML escaped text to the client. This call writes the arguments back to the client's browser after mapping
            all HTML control sequences into safe alternatives. The arguments are converted to strings before writing back to 
            the client and then escaped. The text, written using write, will be buffered up to a configurable maximum. This 
            allows text to be written prior to setting HTTP headers with setHeader.
            @param args Text or objects to write to the client
         */
        function writeHtml(...args): Void
            controller.write(html(args))

        /**
            @duplicate ejs.web::Controller.writeRaw
            @hide
         */
        function writeRaw(...args): Void 
            controller.writeRaw(args)

        /**
            Dump objects for debugging
            @param args List of arguments to print.
         */
        function d(...args): Void {
            write('<pre>\r\n')
            for each (var e: Object in args) {
                write(serialize(e) + "\r\n")
            }
            write('</pre>\r\n')
        }

        /******************************************** Support ***********************************************/

        private function addHelper(fun: Function, overwrite: Boolean = false): Void {
            let name: String = Reflect(fun).name
            if (this[name] && !overwrite) {
                throw new Error('Helper ' + name + ' already exists')
            }
            this[name] = fun
        }

        /*
            Get the view connector to render a control
         */
        private function getConnector(kind: String, options: Object) {
            var connectorName: String
            if (options && options["connector"]) {
                connectorName = options["connector"]
            } else {
                connectorName =  config.view.connectors[kind]
            }
            if (connectorName == undefined || connectorName == null) {
                connectorName =  config.view.connectors["rest"]
                if (connectorName == undefined || connectorName == null) {
                    connectorName = "html"
                }
                config.view.connectors[kind] = connectorName
            }
            let name: String = (connectorName + "Connector").toPascal()
            try {
                return new global[name](controller)
            } catch (e: Error) {
                throw new Error("Undefined view connector: " + name)
            }
        }


        /*
            Update the options based on the model and field being considered
         */
        private function setOptions(field: String, options: Object): Object {
            if (options == null) {
                options = {}
            }
            if (options.fieldName == null) {
                if (currentModel) {
                    options.fieldName = Reflect(currentModel).name.toCamel() + '.' + field
                } else {
                    options.fieldName = field;
                }
            }
            if (options.id == null) {
                if (currentModel && currentModel.id) {
                    options.id = field + '_' + currentModel.id
                }
            }
            options.style ||= field
            if (currentModel && currentModel.hasError(field)) {
                options.style += " -ejs-fieldError"
            }
            return options
        }

        /**
            Format the data for presentation
            @hide
         */
        function getValue(model: Object, field: String, options: Object): String {
            let value
            if (model && field) {
                value = model[field]
            }
            if (value == null || value == undefined) {
                if (options.value) {
                    value = options.value
                }
            }
            if (value == null || value == undefined) {
                value = model
                if (value) {
                    for each (let part in field.split(".")) {
                        value = value[part]
                    }
                }
                if (value == null || value == undefined) {
                    value = ""
                }
            }
            if (options.render != undefined && options.render is Function) {
                fun = options.render
                result = fun(value, model, field).toString()
                return result
            }
            if (options.formatter != undefined && options.formatter is Function) {
                return options.formatter(value).toString()
            }
            let typeName = Reflect(value).typeName

            let fmt
            if (config.view && config.view.formats) {
                fmt = config.view.formats[typeName]
            }
            if (fmt == undefined || fmt == null || fmt == "") {
                return value.toString()
            }
            switch (typeName) {
            case "Date":
                return new Date(value).format(fmt)
            case "Number":
                return fmt.format(value)
            }
            return value.toString()
        }

        /**
            Temporary helper function to format the date. 
            @param fmt Format string
            @returns a function that will return formatted string when invoked
            @stability prototype
            @hide
         */
        function date(fmt: String): Function {
            return function (data: String): String {
                return new Date(data).format(fmt)
            }
        }

        /**
            Temporary helper function to format a number as currency. 
            @param fmt Format string
            @returns a function that will return formatted string when invoked
            @stability prototype
            @hide
         */
        function currency(fmt: String): Function {
            return function (data: String): String {
                return fmt.format(data)
            }
        }

        /**
            Temporary helper function to format a number. 
            @param fmt Format string
            @returns a function that will return formatted string when invoked
            @stability prototype
            @hide
         */
        function number(fmt: String): Function {
            return function (data: String): String {
                return fmt.format(data)
            }
        }

        /*
            Mapping of helper options to HTML attributes ("" value means don't map the name)
         */
        private static const htmlOptions: Object = { 
            background: "", color: "", id: "", height: "", method: "", size: "", 
            style: "class", visible: "", width: "",
        }

        /**
            Map options to a HTML attribute string.
            @param options Optional extra options. See $getOptions for a list of the standard options.
            @returns a string containing the HTML attributes to emit.
            @option background String Background color. This is a CSS RGB color specification. For example "FF0000" for red.
            @option color String Foreground color. This is a CSS RGB color specification. For example "FF0000" for red.
            @option data String URL or action to get live data. The refresh option specifies how often to invoke
                fetch the data.
            @option id String Browser element ID for the control
            @option escape Boolean Escape the text before rendering. This converts HTML reserved tags and delimiters into
                an encoded form.
            @option height (Number|String) Height of the table. Can be a number of pixels or a percentage string. 
                Defaults to unlimited.
            @option method String HTTP method to invoke. May be: GET, POST, PUT or DELETE.
            @option refresh If set, this defines the data refresh period in milliseconds. Only valid if the data option 
                is defined.
            @option size (Number|String) Size of the element.
            @option style String CSS Style to use for the table.
            @option value Default data value to use for the control if not supplied by other means.
            @option visible Boolean Make the control visible. Defaults to true.
            @option width (Number|String) Width of the table or column. Can be a number of pixels or a percentage string. 
         */
        function getOptions(options: Object): String {
            if (!options) {
                return ''
            }
            let result: String = ""
            for (let option: String in options) {
                let mapped = htmlOptions[option]
                if (mapped || mapped == "") {
                    if (mapped == "") {
                        /* No mapping, keep the original option name */
                        mapped = option
                    }
                    result += ' ' +  mapped + '="' + options[option] + '"'
                }
            }
            return result + " "
        }

/*
        private function sortFn(a: Array, ind1: Number, ind2: Number) {
            if (a < b) {
                return -1
            } else if (a > b) {
                return 1
            }
            return 0
        }

        private function sort(data: Array): Array {
            data["sortBy"] = controller.params.sort
            data["sortOrder"] = controller.params.sortOrder
            data.sort(sortFn)
            let tmp = data[0]
            data[0] = data[1]
            data[1] = tmp
            return data;
        }
*/

        /*
            Pivot the data grid. Returns a new grid, original not modified.
         */
        private function pivot(grid: Array, options: Object = {}) {
            if (!grid || grid.length == 0) return grid
            let headers = []
            let i = 0
            for (name in grid[0]) {
                headers[i++] = name
            }
            let table = []
            let row = 0
            table = []
            for each (name in headers) {
                let r = {}
                i = 1
                r[0] = name
                for (j = 0; j < grid.length; j++) {
                    r[i++] = grid[j][name]
                }
                table[row++] = r
            }
            return table
        }

        private function filter(data: Array): Array {
            data = data.clone()
            pattern = controller.params.filter.toLower()
            for (let i = 0; i < data.length; i++) {
                let found: Boolean = false
                for each (f in data[i]) {
                    if (f.toString().toLower().indexOf(pattern) >= 0) {
                        found = true
                    }
                }
                if (!found) {
                    data.remove(i, i)
                    i--
                }
            }
            return data
        }
    }

    internal dynamic class LocalModel implements Record {
        function LocalModel(fields: Object = null) {
            initialize(fields)
        }
    }
}


/*
    @copy   default
    
    Copyright (c) Embedthis Software LLC, 2003-2010. All Rights Reserved.
    Copyright (c) Michael O'Brien, 1993-2010. All Rights Reserved.
    
    This software is distributed under commercial and open source licenses.
    You may use the GPL open source license described below or you may acquire 
    a commercial license from Embedthis Software. You agree to be fully bound 
    by the terms of either license. Consult the LICENSE.TXT distributed with 
    this software for full details.
    
    This software is open source; you can redistribute it and/or modify it 
    under the terms of the GNU General Public License as published by the 
    Free Software Foundation; either version 2 of the License, or (at your 
    option) any later version. See the GNU General Public License for more 
    details at: http://www.embedthis.com/downloads/gplLicense.html
    
    This program is distributed WITHOUT ANY WARRANTY; without even the 
    implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
    
    This GPL license does NOT permit incorporating this software into 
    proprietary programs. If you are unable to comply with the GPL, you must
    acquire a commercial license to use this software. Commercial licenses 
    for this software and support services are available from Embedthis 
    Software at http://www.embedthis.com 
    
    @end
 */
/************************************************************************/
/*
 *  End of file "../src/es/web/View.es"
 */
/************************************************************************/



/************************************************************************/
/*
 *  Start of file "../src/es/xml/XML.es"
 */
/************************************************************************/

/*
 *  XML.es - XML class
 *
 *  Copyright (c) All Rights Reserved. See details at the end of the file.
 */

module ejs {

    use default namespace intrinsic

    /**
     *  The XML class provides a simple ability to load, parse and save XML documents.
     *  @spec ejs
     */
    native final class XML extends Object {

        use default namespace public

        /**
         *  XML Constructor. Create an empty XML object.
         *  @param value An optional XML or XMLList object to clone.
         */
        native function XML(value: Object = null)

        /**
         *  Load an XML document
         *  @param filename Name of the file containing the XML document to load
         */
        native function load(filename: String): Void

        /**
         *  Save the XML object to a file
         *  @param filename Name of the file to save the XML document to
         */
        native function save(filename: String): Void

        # FUTURE
        static var ignoreComments: Boolean

        # FUTURE
        static var ignoreProcessingInstructions: Boolean

        # FUTURE
        static var ignoreWhitespace: Boolean

        # FUTURE
        static var prettyPrinting: Boolean

        # FUTURE
        static var prettyIndent: Boolean

        # FUTURE
        native function addNamespace(ns: Namespace): XML

        /**
         *  Append a child to this XML object.
         *  @param child The child to add.
         *  @return This object with the added child.
         */
        # FUTURE
        native function appendChild(child: XML): XML

        /**
         *  Get an XMLList containing all of the attributes of this object with the given name.
         *  @param name The name to search on.
         *  @return An XMLList with all the attributes (zero or more).
         */
        function attribute(name: String): XMLList
            this["@" + name]

        /**
         *  Get an XMLList containing all of the attributes of this object.
         *  @return An XMLList with all the attributes (zero or more).
         */
        function attributes(): XMLList
            this.@*
        
        /**
         *  Get an XMLList containing the list of children (properties) in this XML object with the given name.
         *  @param name The name to search on.
         *  @return An XMLList with all the children names (zero or more).
         */
        # FUTURE
        native function child(name: String): XMLList
        
        /**
         *  Get the position of this object within the context of its parent.
         *  @return A number with the zero-based position.
         */
        # FUTURE
        native function childIndex(): Number
        
        /**
         *  Get an XMLList containing the children (properties) of this object in order.
         *  @return An XMLList with all the properties.
         */
        # FUTURE
        native function children(): XMLList

        /**
         *  Get an XMLList containing the properties of this object that are
         *  comments.
         *  @return An XMLList with all the comment properties.
         */
        # FUTURE
        native function comments(): XMLList
        
        /**
         *  Compare an XML object to another one or an XMLList with only one
         *  XML object in it. If the comparison operator is true, then one
         *  object is said to contain the other.
         *  @return True if this object contains the argument.
         */
        # FUTURE
        native function contains(obj: Object): Boolean

        /**
         *  Deep copy an XML object. The new object has its parent set to null.
         *  @return Then new XML object.
         */
        # FUTURE
        native function copy(): XML

        /**
         *  Get the defaults settings for an XML object. The settings include boolean values for: ignoring comments, 
         *  ignoring processing instruction, ignoring white space and pretty printing and indenting. See ECMA-357
         *  for details.
         *  @return Get the settings for this XML object.
         */
        # FUTURE
        native function defaultSettings(): Object

        /**
         *  Get all the descendants (that have the same name) of this XML object. The optional argument defaults 
         *  to getting all descendants.
         *  @param name The (optional) name to search on.
         *  @return The list of descendants.
         */
        # FUTURE
        native function descendants(name: String = "*"): Object
        
        /**
         *  Get all the children of this XML node that are elements having the
         *  given name. The optional argument defaults to getting all elements.
         *  @param name The (optional) name to search on.
         *  @return The list of elements.
         */
        function elements(name: String = "*"): XMLList
            this[name]

        /**
         *  Get an iterator for this node to be used by "for (v in node)"
         *  @return An iterator object.
         */
        override iterator native function get(): Iterator

        /**
         *  Get an iterator for this node to be used by "for each (v in node)"
         *  @return An iterator object.
         */
        override iterator native function getValues(): Iterator

        /**
         *  Determine whether this XML object has complex content. If the object has child elements it is 
         *  considered complex.
         *  @return True if this object has complex content.
         */
        # FUTURE
        native function hasComplexContent(): Boolean

        /**
         *  Determine whether this object has its own property of the given name.
         *  @param prop The property to look for.
         *  @return True if this object does have that property.
         */
        # FUTURE
        override intrinsic native function hasOwnProperty(name: String): Boolean
        
        /**
         *  Determine whether this XML object has simple content. If the object
         *  is a text node, an attribute node or an XML element that has no
         *  children it is considered simple.
         *  @return True if this object has simple content.
         */
        # FUTURE
        native function hasSimpleContent(): Boolean

        # FUTURE
        native function inScopeNamespaces(): Array

        /**
         *  Insert a child object into an XML object immediately after a specified marker object. If the marker object 
         *  is null then the new object is inserted at the end. If the marker object is not found then the insertion 
         *  is not made.
         *  @param marker Insert the new element before this one.
         *  @param child Child element to be inserted.
         *  @return This XML object - modified or not.
         */
        # FUTURE
        native function insertChildAfter(marker: Object, child: Object): XML

        /**
         *  Insert a child object into an XML object immediately before a specified marker object. If the marker 
         *  object is null then the new object is inserted at the end. If the marker object is not found then the
         *  insertion is not made.
         *  @param marker Insert the new element before this one.
         *  @param child Child element to be inserted.
         *  @return This XML object - modified or not.
         */
        # FUTURE
        native function insertChildBefore(marker: Object, child: Object): XML

        /**
         *  Return the length of an XML object in elements
         *  NOTE: This is a method and must be invoked with ().
         *  @return A number indicating the number of child elements.
         */
        override native function length(): Number
        
        /**
         *  Get the local name portion of the complete name of this XML object.
         *  @return The local name.
         */
        # FUTURE
        native function localName(): String

        /**
         *  Get the qualified name of this XML object.
         *  @return The qualified name.
         */
        native function name(): String

        # FUTURE
        native function namespace(prefix: String = null): Object

        # FUTURE
        native function namespaceDeclarations(): Array

        /**
         *  Get the kind of node this XML object is.
         *  @return The node kind.
         */
        # FUTURE
        native function nodeKind(): String
        
        /**
         *  Merge adjacent text nodes into one text node and eliminate empty text nodes.
         *  @return This XML object.
         */
        # FUTURE
        native function normalize(): XML
        
        /**
         *  Get the parent of this XML object.
         *  @return The parent.
         */
        native function parent(): XML

        /**
         *  Insert a child object into an XML object immediately before all existing properties.
         *  @param child Child element to be inserted.
         *  @return This XML object - modified or not.
         */
        # FUTURE
        native function prependChild(child: Object): XML
        
        /**
         *  Get all the children of this XML node that are processing instructions having the given name. 
         *  The optional argument defaults to getting all processing instruction nodes.
         *  @param name The (optional) name to search on.
         *  @return The list of processing instruction nodes.
         */
        # FUTURE
        native function processingInstructions(name: String = "*"): XMLList

        /**
         *  Test a property to see if it will be included in an enumeration over the XML object.
         *  @param property The property to test.
         *  @return True if the property will be enumerated.
         */
        # FUTURE
        override intrinsic native function propertyIsEnumerable(property: Object): Boolean

        /**
         *  Change the value of a property. If the property is not found, nothing happens.
         *  @param property The property to change.
         *  @param value The new value.
         *  @return True if the property will be enumerated.
         */
        # FUTURE
        native function replace(property: Object, value: Object): void
        
        /**
         *  Replace all the current properties of this XML object with a new set. The argument can be 
         *  another XML object or an XMLList.
         *  @param properties The new property or properties.
         *  @return This XML object.
         */
        # FUTURE
        native function setChildren(properties: Object): XML
        
        /**
         *  Set the local name portion of the complete name of this XML object.
         *  @param The local name.
         */
        # FUTURE
        native function setLocalName(name: String): void

        /**
         *  Set the qualified name of this XML object.
         *  @param The qualified name.
         */
        # FUTURE
        native function setName(name: String): void

        /**
         *  Get the settings for this XML object. The settings include boolean values for: ignoring comments, 
         *  ignoring processing instruction, ignoring white space and pretty printing and indenting. See ECMA-357
         *  for details.
         *  @return Get the settings for this XML object.
         */
        # FUTURE
        native function settings(): Object
        
        /**
         *  Configure the settings for this XML object.
         *  @param settings A "settings" object previously returned from a call to the "settings" method.
         */
        # FUTURE
        native function setSettings(settings: Object): void

        /**
         *  Get all the properties of this XML node that are text nodes having the given name. The optional 
        *   argument defaults to getting all text nodes.
         *  @param name The (optional) name to search on.
         *  @return The list of text nodes.
         */
        # FUTURE
        native function text(name: String = "*"): XMLList

        /**
         *  Convert to a JSON encoding
         *  @return A JSON string 
         */
        override native function toJSON(): String 

        /**
         *  Provides an XML-encoded version of the XML object that includes the tags.
         *  @return A string with the encoding.
         */
        # FUTURE
        native function toXMLString(): String 

        /**
         *  Provides a string representation of the XML object.
         *  @return A string with the encoding.
         */
        override native function toString(): String 
        
        /**
         *  Return this XML object.
         *  @return This object.
         */
        # FUTURE 
        override intrinsic function valueOf(): XML {
            return this
        }
    }
}


/*
 *  @copy   default
 *  
 *  Copyright (c) Embedthis Software LLC, 2003-2010. All Rights Reserved.
 *  Copyright (c) Michael O'Brien, 1993-2010. All Rights Reserved.
 *  
 *  This software is distributed under commercial and open source licenses.
 *  You may use the GPL open source license described below or you may acquire 
 *  a commercial license from Embedthis Software. You agree to be fully bound 
 *  by the terms of either license. Consult the LICENSE.TXT distributed with 
 *  this software for full details.
 *  
 *  This software is open source; you can redistribute it and/or modify it 
 *  under the terms of the GNU General Public License as published by the 
 *  Free Software Foundation; either version 2 of the License, or (at your 
 *  option) any later version. See the GNU General Public License for more 
 *  details at: http://www.embedthis.com/downloads/gplLicense.html
 *  
 *  This program is distributed WITHOUT ANY WARRANTY; without even the 
 *  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
 *  
 *  This GPL license does NOT permit incorporating this software into 
 *  proprietary programs. If you are unable to comply with the GPL, you must
 *  acquire a commercial license to use this software. Commercial licenses 
 *  for this software and support services are available from Embedthis 
 *  Software at http://www.embedthis.com 
 *  
 */
/************************************************************************/
/*
 *  End of file "../src/es/xml/XML.es"
 */
/************************************************************************/



/************************************************************************/
/*
 *  Start of file "../src/es/xml/XMLList.es"
 */
/************************************************************************/

/*
 *  XMLList.es - XMLList class

 *  Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */

module ejs {

    use default namespace intrinsic

    /**
     *  The XMLList class is a helper class for the XML class.
     *  @spec ecma-357
     *  @hide
     */
    native final class XMLList extends Object {

        use default namespace public

        /**
         *  XML Constructor. Create an empty XML object.
         */
        native function XMLList() 

        # FUTURE
        native function addNamespace(ns: Namespace): XML

        /**
         *  Append a child to this XML object.
         *  @param child The child to add.
         *  @return This object with the added child.
         */
        # FUTURE
        native function appendChild(child: XML): XML

        /**
         *  Get an XMLList containing all of the attributes of this object with the given name.
         *  @param name The name to search on.
         *  @return An XMLList with all the attributes (zero or more).
         */
        function attribute(name: String): XMLList
            this["@" + name]

        /**
         *  Get an XMLList containing all of the attributes of this object.
         *  @return An XMLList with all the attributes (zero or more).
         */
        function attributes(): XMLList
            this.@*

        /**
         *  Get an XMLList containing the list of children (properties) in this XML object with the given name.
         *  @param name The name to search on.
         *  @return An XMLList with all the children names (zero or more).
         */
        # FUTURE
        native function child(name: String): XMLList

        /**
         *  Get the position of this object within the context of its parent.
         *  @return A number with the zero-based position.
         */
        # FUTURE
        native function childIndex(): Number

        /**
         *  Get an XMLList containing the children (properties) of this object in order.
         *  @return An XMLList with all the properties.
         */
        # FUTURE
        native function children(): XMLList
        
        /**
         *  Get an XMLList containing the properties of this object that are
         *  comments.
         *  @return An XMLList with all the comment properties.
         */
        # FUTURE
        native function comments(): XMLList
        
        /**
         *  Compare an XML object to another one or an XMLList with only one XML object in it. If the 
         *  comparison operator is true, then one object is said to contain the other.
         *  @return True if this object contains the argument.
         */
        # FUTURE
        native function contains(obj: Object): Boolean

        /**
         *  Deep copy an XML object. The new object has its parent set to null.
         *  @return Then new XML object.
         */
        # FUTURE
        native function copy(): XML

        /**
         *  Get the defaults settings for an XML object. The settings include boolean values for: ignoring comments, 
         *  ignoring processing instruction, ignoring white space and pretty printing and indenting. See ECMA-357
         *  for details.
         *  @return Get the settings for this XML object.
         */
        # FUTURE
        native function defaultSettings(): Object

        /**
         *  Get all the descendants (that have the same name) of this XML object. The optional argument defaults 
         *  to getting all descendants.
         *  @param name The (optional) name to search on.
         *  @return The list of descendants.
         */
        # FUTURE
        native function descendants(name: String = "*"): Object

        /**
         *  Get all the children of this XML node that are elements having the
         *  given name. The optional argument defaults to getting all elements.
         *  @param name The (optional) name to search on.
         *  @return The list of elements.
         */
        function elements(name: String = "*"): XMLList
            this[name]

        /**
         *  Get an iterator for this node to be used by "for (v in node)"
         *  @return An iterator object.
         */
        override iterator native function get(): Iterator

        /**
         *  Get an iterator for this node to be used by "for each (v in node)"
         *  @return An iterator object.
         */
        override iterator native function getValues(): Iterator

        /**
         *  Determine whether this XML object has complex content. If the object has child elements it is 
         *  considered complex.
         *  @return True if this object has complex content.
         */
        # FUTURE
        native function hasComplexContent(): Boolean

        /**
         *  Determine whether this object has its own property of the given name.
         *  @param prop The property to look for.
         *  @return True if this object does have that property.
         */
        # FUTURE
        override intrinsic native function hasOwnProperty(name: String): Boolean

        /**
         *  Determine whether this XML object has simple content. If the object
         *  is a text node, an attribute node or an XML element that has no
         *  children it is considered simple.
         *  @return True if this object has simple content.
         */
        # FUTURE
        native function hasSimpleContent(): Boolean

        /**
         *  Return the namespace in scope
         *  @return Array of namespaces
         */
        # FUTURE
        native function inScopeNamespaces(): Array

        /**
         *  Insert a child object into an XML object immediately after a specified marker object. If the marker 
         *  object is null then the new object is inserted at the beginning. If the marker object is not found 
         *  then the insertion is not made.
         *  @param marker Insert the new element after this one.
         *  @param child Child element to be inserted.
         *  @return This XML object - modified or not.
         */
        # FUTURE
        native function insertChildAfter(marker: Object, child: Object): XML

        /**
         *  Insert a child object into an XML object immediately before a specified marker object. If the marker 
         *  object is null then the new object is inserted at the end. If the marker object is not found then the
         *  insertion is not made.
         *  @param marker Insert the new element before this one.
         *  @param child Child element to be inserted.
         *  @return This XML object - modified or not.
         */
        # FUTURE
        native function insertChildBefore(marker: Object, child: Object): XML

        /**
         *  Return the length of an XML object in elements. NOTE: This is a method and must be invoked with ().
         *  @return A number indicating the number of child elements.
         */
        override native function length(): Number

        /**
         *  Get the local name portion of the complete name of this XML object.
         *  @return The local name.
         */
        # FUTURE
        native function localName(): String

        /**
         *  Get the qualified name of this XML object.
         *  @return The qualified name.
         */
        native function name(): String

        # FUTURE
        native function namespace(prefix: String = null): Object

        # FUTURE
        native function namespaceDeclarations(): Array

        /**
         *  Get the kind of node this XML object is.
         *  @return The node kind.
         */
        # FUTURE
        native function nodeKind(): String

        /**
         *  Merge adjacent text nodes into one text node and eliminate empty text nodes.
         *  @return This XML object.
         */
        # FUTURE
        native function normalize(): XML

        /**
         *  Get the parent of this XML object.
         *  @return The parent.
         */
        native function parent(): XML

        /**
         *  Insert a child object into an XML object immediately before all existing properties.
         *  @param child Child element to be inserted.
         *  @return This XML object - modified or not.
         */
        # FUTURE
        native function prependChild(child: Object): XML

        /**
         *  Get all the children of this XML node that are processing instructions having the given name. 
         *  The optional argument defaults to getting all processing instruction nodes.
         *  @param name The (optional) name to search on.
         *  @return The list of processing instruction nodes.
         */
        # FUTURE
        native function processingInstructions(name: String = "*"): XMLList

        /**
         *  Test a property to see if it will be included in an enumeration over the XML object.
         *  @param property The property to test.
         *  @return True if the property will be enumerated.
         */
        # ECMA
        override intrinsic native function propertyIsEnumerable(property: Object): Boolean

        /**
         *  Change the value of a property. If the property is not found, nothing happens.
         *  @param property The property to change.
         *  @param value The new value.
         *  @return True if the property will be enumerated.
         */
        # FUTURE
        native function replace(property: Object, value: Object): void
        
        /**
         *  Replace all the current properties of this XML object with a new set. The argument can be 
         *  another XML object or an XMLList.
         *  @param properties The new property or properties.
         *  @return This XML object.
         */
        # FUTURE
        native function setChildren(properties: Object): XML

        /**
         *  Set the local name portion of the complete name of this XML object.
         *  @param The local name.
         */
        # FUTURE
        native function setLocalName(name: String): void

        /**
         *  Set the qualified name of this XML object.
         *  @param The qualified name.
         */
        # FUTURE
        native function setName(name: String): void

        /**
         *  Get the settings for this XML object. The settings include boolean values for: ignoring comments, 
         *  ignoring processing instruction, ignoring white space and pretty printing and indenting. See ECMA-357
         *  for details.
         *  @return Get the settings for this XML object.
         */
        # FUTURE
        native function settings(): Object

        /**
         *  Configure the settings for this XML object.
         *  @param settings A "settings" object previously returned from a call to the "settings" method.
         */
        # FUTURE
        native function setSettings(settings: Object): void
        
        /**
         *  Get all the properties of this XML node that are text nodes having the given name. The optional 
        *   argument defaults to getting all text nodes.
         *  @param name The (optional) name to search on.
         *  @return The list of text nodes.
         */
        # FUTURE
        native function text(name: String = "*"): XMLList
        
        /**
         *  Convert to a JSON encoding
         *  @return A JSON string 
         */
        override native function toJSON(): String 

        /**
         *  Provides a string representation of the XML object.
         *  @return A string with the encoding.
         */
        override native function toString(): String 

        /**
         *  Provides an XML-encoded version of the XML object that includes the tags.
         *  @return A string with the encoding.
         */
        # FUTURE
        native function toXMLString(): String 

        /**
         *  Return this XML object.
         *  @return This object.
         */
        # FUTURE
        override intrinsic function valueOf(): XML {
            return this
        }

        /*
           XML
                .prototype
                .ignoreComments
                .ignoreProcessingInstructions
                .ignoreWhitespace
                .prettyPrinting
                .prettyIndent
                .settings()
                .setSettings(settings)
                .defaultSettings()

               function domNode()
               function domNodeList()
               function xpath(XPathExpression)
           XMLList
               function domNode()
               function domNodeList()
               function xpath(XPathExpression)
         */
    }
}


/*
 *  @copy   default
 *  
 *  Copyright (c) Embedthis Software LLC, 2003-2010. All Rights Reserved.
 *  
 *  This software is distributed under commercial and open source licenses.
 *  You may use the GPL open source license described below or you may acquire 
 *  a commercial license from Embedthis Software. You agree to be fully bound 
 *  by the terms of either license. Consult the LICENSE.TXT distributed with 
 *  this software for full details.
 *  
 *  This software is open source; you can redistribute it and/or modify it 
 *  under the terms of the GNU General Public License as published by the 
 *  Free Software Foundation; either version 2 of the License, or (at your 
 *  option) any later version. See the GNU General Public License for more 
 *  details at: http://www.embedthis.com/downloads/gplLicense.html
 *  
 *  This program is distributed WITHOUT ANY WARRANTY; without even the 
 *  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
 *  
 *  This GPL license does NOT permit incorporating this software into 
 *  proprietary programs. If you are unable to comply with the GPL, you must
 *  acquire a commercial license to use this software. Commercial licenses 
 *  for this software and support services are available from Embedthis 
 *  Software at http://www.embedthis.com 
 *  
 *  @end
 */

/************************************************************************/
/*
 *  End of file "../src/es/xml/XMLList.es"
 */
/************************************************************************/

