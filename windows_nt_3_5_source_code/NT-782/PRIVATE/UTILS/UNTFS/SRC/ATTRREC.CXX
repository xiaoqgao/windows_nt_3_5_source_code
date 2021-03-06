/*++

Copyright (c) 1991	Microsoft Corporation

Module Name:

	attrrec.hxx

Abstract:

	This module contains the member function definitions for
	NTFS_ATTRIBUTE_RECORD, which models NTFS attribute records.

	An Attribute Record may be a template laid over a chunk of
	memory; in that case, it does not own the memory.  It may
	also be told, upon initialization, to allocate its own memory
	and copy the supplied data.  In that case, it is also responsible
	for freeing that memory.

	Attribute Records are passed between Attributes and File
	Record Segments.  A File Record Segment can initialize
	an Attribute with a list of Attribute Records; when an
	Attribute is Set into a File Record Segment, it packages
	itself up into Attribute Records and inserts them into
	the File Record Segment.

	File Record Segments also use Attribute Records to scan
	through their list of attribute records, and to shuffle
	them around.

Author:

	Bill McJohn (billmc) 14-June-91

Environment:

	ULIB, User Mode

--*/

#include <pch.cxx>

#define _NTAPI_ULIB_
#define _UNTFS_MEMBER_

#include "ulib.hxx"
#include "error.hxx"
#include "untfs.hxx"

#include "wstring.hxx"
#include "extents.hxx"
#include "attrrec.hxx"
#include "attrcol.hxx"
#include "ntfsbit.hxx"
#include "extents.hxx"

#include "upcase.hxx"


DEFINE_EXPORTED_CONSTRUCTOR( NTFS_ATTRIBUTE_RECORD, OBJECT, UNTFS_EXPORT );

UNTFS_EXPORT
NTFS_ATTRIBUTE_RECORD::~NTFS_ATTRIBUTE_RECORD(
	)
{
	Destroy();
}

VOID
NTFS_ATTRIBUTE_RECORD::Construct(
	)
/*++

Routine Description:

	This method is the private worker function for object construction.

Arguments:

	None.

Return Value:

	None.

--*/
{
	_Data = NULL;
	_MaximumLength = 0;
    _IsOwnBuffer = FALSE;
}

VOID
NTFS_ATTRIBUTE_RECORD::Destroy(
	)
/*++

Routine Description:

	This method is the private worker function for object destruction.

Arguments:

	None.

Return Value:

	None.

--*/
{
	if( _IsOwnBuffer && _Data != NULL ) {

		FREE( _Data );
	}

	_Data = NULL;
	_MaximumLength = 0;
    _IsOwnBuffer = FALSE;
}


BOOLEAN
NTFS_ATTRIBUTE_RECORD::Initialize(
	IN OUT  PVOID   Data,
	IN      ULONG   MaximumLength,
	IN      BOOLEAN MakeCopy
	)
/*++

Routine Description:

	This method initializes an NTFS_ATTRIBUTE_RECORD object,
	handing it a buffer with attribute record data.  The caller
	may also ask the object to make a private copy of the data.

Arguments:

	Data			-- supplies a buffer containing the attribute
						record data the object will own.
	MaximumLength	-- supplies the size of the buffer.
	MakeCopy		-- supplies a flag indicating whether the object
						should copy the data to a private buffer.

Return Value:

	TRUE upon successful completion.

Notes:

	If MakeCopy is TRUE, then the object must allocate its own
	buffer and copy the attribute record data to it, in which
	case the object is also responsible for freeing that private
	buffer.  It that flag is FALSE, then the object will cache a
	pointer to the buffer supplied by the client; the client is
	responsible for making sure that buffer remains valid for
	the lifetime of the NTFS_ATTRIBUTE_RECORD object.

	This object is reinitializable.

--*/
{
    Destroy();

	if( !MakeCopy ) {

		_Data = (PATTRIBUTE_RECORD_HEADER) Data;
		_MaximumLength = MaximumLength;
		_IsOwnBuffer = FALSE;

		return TRUE;

	} else {

		if( (_Data = (PATTRIBUTE_RECORD_HEADER)
					 MALLOC( (UINT) MaximumLength )) == NULL ) {

			Destroy();
			return FALSE;
		}

		_MaximumLength = MaximumLength;
		_IsOwnBuffer = TRUE;
        memcpy(_Data, Data, (UINT) MaximumLength);

		return TRUE;
	}
}


UNTFS_EXPORT
BOOLEAN
NTFS_ATTRIBUTE_RECORD::Initialize(
    IN OUT  PVOID   Data
	)
/*++

Routine Description:

    This version of Initialize takes it's maximum size from the
    attribute record.

Arguments:

    Data    - supplies a buffer containing the attribute
                record data.

Return Value:

	TRUE upon successful completion.

--*/
{
    Destroy();

    _Data = (PATTRIBUTE_RECORD_HEADER) Data;
    _MaximumLength = _Data->RecordLength;
    _IsOwnBuffer = FALSE;

    return TRUE;
}

BOOLEAN
NTFS_ATTRIBUTE_RECORD::CreateResidentRecord(
	IN  PCVOID              Value,
	IN  ULONG               ValueLength,
	IN  ATTRIBUTE_TYPE_CODE TypeCode,
	IN  PCWSTRING           Name,
	IN  USHORT              Flags,
	IN  UCHAR               ResidentFlags
	)
/*++

Routine Description:

	This method formats the object's buffer with a resident
	attribute record.

Arguments:

	Value			-- supplies the attribute value
	ValueLength 	-- supplies the length of the value
	TypeCode		-- supplies the attribute type code
	Name			-- supplies the name of the attribute
						(may be NULL)
	Flags			-- supplies the attribute's flags.
	ResidentFlags	-- supplies the attribute's resident flags

Return Value:

	TRUE upon successful completion.

--*/
{
    // Clear the memory first.
    memset(_Data, 0, (UINT) _MaximumLength);

	// We will arrange the attribute in the following order:
	//	Attribute Record Header
	//	Name (if any)
	//	Value

	if( _MaximumLength < SIZE_OF_RESIDENT_HEADER )	{

		DbgAbort( "Create:  buffer is too small.\n" );
		return FALSE;
	}

	_Data->TypeCode = TypeCode;
	_Data->FormCode = RESIDENT_FORM;
	_Data->Flags = Flags;

	if( Name != NULL ) {

		_Data->NameLength = (UCHAR) Name->QueryChCount();
		_Data->NameOffset = DwordAlign(SIZE_OF_RESIDENT_HEADER);

		_Data->Form.Resident.ValueOffset =
			DwordAlign( _Data->NameOffset +
						_Data->NameLength * sizeof( WCHAR ) );

	} else {

		_Data->NameLength = 0;
		_Data->NameOffset = 0;

		_Data->Form.Resident.ValueOffset =
                    DwordAlign(SIZE_OF_RESIDENT_HEADER);
	}

	_Data->Form.Resident.ValueLength = ValueLength;
	_Data->Form.Resident.ResidentFlags = ResidentFlags;

	_Data->RecordLength =
		QuadAlign(_Data->Form.Resident.ValueOffset + ValueLength );

	if( _Data->RecordLength > _MaximumLength ) {

		DbgPrint( "Create:  Buffer is too small for name and value.\n" );
		return FALSE;
	}

	// Now that we're sure there's room, copy the name (if any)
	// and the value into their respective places.

    if( Name != NULL ) {

        Name->QueryWSTR( 0,
                         _Data->NameLength,
                         (PWSTR)((PBYTE)_Data + _Data->NameOffset),
                         _Data->NameLength,
                         FALSE );
	}

	memcpy( (PBYTE)_Data + _Data->Form.Resident.ValueOffset,
			Value,
			(UINT) ValueLength );

	return TRUE;
}


BOOLEAN
NTFS_ATTRIBUTE_RECORD::CreateNonresidentRecord(
	IN  PCNTFS_EXTENT_LIST  Extents,
	IN  BIG_INT             AllocatedLength,
	IN  BIG_INT             ActualLength,
	IN  BIG_INT             ValidLength,
	IN  ATTRIBUTE_TYPE_CODE TypeCode,
	IN  PCWSTRING           Name,
	IN  USHORT              Flags
	)
/*++

Routine Description:

	This method formats the attribute record to hold a nonresident
	attribute.

Arguments:

	Extents 		-- supplies an extent list describing the
						attribute value's disk storage.
	AllocatedLength -- supplies the allocated length of the value
	ActualLength	-- supplies the actual length of the value
	ValidLength 	-- supplies the valid length of the value
	TypeCode		-- supplies the attribute type code
	Name			-- supplies the name of the attribute
						(may be NULL)
	Flags			-- supplies the attribute's flags.

--*/
{
    ULONG   MappingPairsLength;
    VCN     NextVcn, HighestVcn;

    // Clear the memory first.
    memset(_Data, 0, (UINT) _MaximumLength);

	// We will arrange the attribute in the following order:
	//	Attribute Record Header
	//	Name (if any)
	//	Compressed Mapping Pairs

	if( _MaximumLength < SIZE_OF_NONRESIDENT_HEADER )	{

		DbgAbort( "Create:  buffer is too small.\n" );
		return FALSE;
	}

	_Data->TypeCode = TypeCode;
	_Data->FormCode = NONRESIDENT_FORM;
	_Data->Flags = Flags;

	if( Name != NULL ) {

		_Data->NameLength = (UCHAR) Name->QueryChCount();
		_Data->NameOffset = DwordAlign(SIZE_OF_NONRESIDENT_HEADER);

		_Data->Form.Nonresident.MappingPairsOffset =
			(USHORT)DwordAlign( _Data->NameOffset +
								_Data->NameLength * sizeof( WCHAR ) );

	} else {

		_Data->NameLength = 0;
		_Data->NameOffset = 0;

		_Data->Form.Nonresident.MappingPairsOffset =
			(USHORT)DwordAlign(SIZE_OF_NONRESIDENT_HEADER);
	}

	_Data->Form.Nonresident.AllocatedLength =
                    AllocatedLength.GetLargeInteger();

	_Data->Form.Nonresident.FileSize = ActualLength.GetLargeInteger();
	_Data->Form.Nonresident.ValidDataLength = ValidLength.GetLargeInteger();


	// Copy the name

	if( Name != NULL ) {

		if( _Data->NameOffset + _Data->NameLength > _MaximumLength ) {

			// There isn't enough room for the name.

			return FALSE;
		}

        Name->QueryWSTR( 0,
                         _Data->NameLength,
                         (PWSTR)((PBYTE)_Data + _Data->NameOffset),
                         _Data->NameLength,
                         FALSE );
    }


	if( !Extents->QueryCompressedMappingPairs(
                        (PVCN)&(_Data->Form.Nonresident.LowestVcn),
                        &NextVcn,
                        &MappingPairsLength,
                        _MaximumLength -
						  _Data->Form.Nonresident.MappingPairsOffset,
						(PVOID)((PBYTE)_Data +
						  _Data->Form.Nonresident.MappingPairsOffset) ) ) {

		// Unable to get the compressed mapping pairs.

        DbgPrint( "Could not get compressed mapping pairs.\n" );
		return FALSE;
    }

    HighestVcn = NextVcn - 1;
    memcpy( &_Data->Form.Nonresident.HighestVcn, &HighestVcn, sizeof(VCN) );

	_Data->RecordLength =
		QuadAlign(_Data->Form.Nonresident.MappingPairsOffset +
				  MappingPairsLength );

	return TRUE;
}


BOOLEAN
NTFS_ATTRIBUTE_RECORD::Verify(
    IN  PCNTFS_ATTRIBUTE_COLUMNS    AttributeDefTable,
    IN  BOOLEAN                     BeLenient
    ) CONST
/*++

Routine Description:

    This routine verifies an attribute record for consistency against
    itself and against the attribute definition table.  This routine
    will return FALSE if the attribute record contains any
    inconsistencies.

Arguments:

    AttributeDefTable   - Supplies the attribute definition table.

Return Value:

    FALSE   - The attribute record is inconsistent.
    TRUE    - The attribute record is ok.

--*/
{
    NTFS_EXTENT_LIST    extent_list;
    BOOLEAN             bad_mapping_pairs;
    ULONG               index;
    ULONG               column_flags;
    BIG_INT             length;
	PFILE_NAME			file_name;
	ULONG				value_length;
    UCHAR               i;
    PWCHAR              p;

    DbgAssert(_Data);

    // Make sure that we can access at least the form code.

    if (FIELD_OFFSET(ATTRIBUTE_RECORD_HEADER, Instance) > _Data->RecordLength) {
        return FALSE;
    }

    // Make sure that the form code is either resident or non-resident.

    if (_Data->FormCode != RESIDENT_FORM &&
        _Data->FormCode != NONRESIDENT_FORM) {

        DbgPrintf("Attribute %d has non-existent form code.\n", _Data->TypeCode);
        return FALSE;
    }


    // Make sure that the record is at least as big as the header
    // for the record

    if (_Data->FormCode == RESIDENT_FORM &&
        _Data->RecordLength < SIZE_OF_RESIDENT_HEADER) {

        return FALSE;
    }

    if (_Data->FormCode == NONRESIDENT_FORM &&
        _Data->RecordLength < SIZE_OF_NONRESIDENT_HEADER) {

        return FALSE;
    }


    switch (_Data->TypeCode) {

        case $STANDARD_INFORMATION:

            if (!IsResident() ||
                _Data->Form.Resident.ValueLength <
                sizeof(STANDARD_INFORMATION)) {

                // This attribute must be resident and at least
                // as big as the above structure.

                DbgPrintf("The standard information is too small\n");
                return FALSE;
            }

            // Fall through for next check.

        case $ATTRIBUTE_LIST:
        case $VOLUME_VERSION:
        case $SECURITY_DESCRIPTOR:
        case $VOLUME_NAME:
        case $VOLUME_INFORMATION:
        case $SYMBOLIC_LINK:
        case $EA_INFORMATION:
        case $EA_DATA:

            if (_Data->NameLength) {

                // These attribute may not have names.
                DbgPrintf("Attribute %d should not have a name.\n", _Data->TypeCode);
                return FALSE;
            }

            break;

        case $INDEX_ALLOCATION:

            // $INDEX_ALLOCATION's can't be resident.

            if (IsResident()) {
                return FALSE;
            }
            break;

        default:
            break;

    }


    // Make sure the name offset if well-aligned and in bounds.
    // Also make sure that the name does not have any unicode NULLs
    // in them.

    if (_Data->NameLength) {

        if (_Data->NameOffset%sizeof(WCHAR) ||
            _Data->NameOffset + _Data->NameLength > _Data->RecordLength) {

            DbgPrintf("Corrupt name for attribute %d.\n", _Data->TypeCode);
            return FALSE;
        }

        p = (PWCHAR) ((PCHAR) _Data + _Data->NameOffset);
        for (i = 0; i < _Data->NameLength; i++) {
            if (!p[i]) {
                DbgPrintf("Unicode NULL in attribute name for attribute %d.\n",
                          _Data->TypeCode);
                return FALSE;
            }
        }
    }


    // Make sure that things mesh with the attribute definition table.

    if (AttributeDefTable) {


        if (!AttributeDefTable->QueryIndex(_Data->TypeCode, &index)) {

            // The attribute type code doesn't exist in the attribute
            // definition table.

            DbgPrintf("Attribute %d does not exist in the definition table.\n", _Data->TypeCode);
            return FALSE;
        }

        column_flags = AttributeDefTable->QueryFlags(index);

        if (IsResident() &&
            (_Data->Form.Resident.ResidentFlags & RESIDENT_FORM_INDEXED) &&
            !(column_flags & ATTRIBUTE_DEF_INDEXABLE)) {

            // Non-indexable indexed attribute.

            DbgPrintf("Attribute %d is NOT indexable.\n", _Data->TypeCode);
            return FALSE;
        }

        if ((column_flags & ATTRIBUTE_DEF_MUST_BE_INDEXED) &&
            !(IsResident() &&
              (_Data->Form.Resident.ResidentFlags & RESIDENT_FORM_INDEXED))) {

            // Attribute must be indexed but isn't.

            DbgPrintf("Attribute %d is MUST be indexed.\n", _Data->TypeCode);
            return FALSE;
        }

        if ((column_flags & ATTRIBUTE_DEF_INDEXABLE) && _Data->NameLength) {

            // Indexable attributes cannot have names.

            DbgPrintf("Attribute %d cannot have a name.\n", _Data->TypeCode);
            return FALSE;
        }

        if ((column_flags & ATTRIBUTE_DEF_MUST_BE_NAMED) &&
            !_Data->NameLength) {

            // Attribute must be named but isn't.

            DbgPrintf("Attribute %d MUST have a name.\n", _Data->TypeCode);
            return FALSE;
        }

        if ((column_flags & ATTRIBUTE_DEF_MUST_BE_RESIDENT) &&
            !IsResident()) {

            // Attribute must be resident but isn't.

            DbgPrintf("Attribute %d MUST be resident.\n", _Data->TypeCode);
            return FALSE;
        }

        if (IsResident()) {
            length = _Data->Form.Resident.ValueLength;
        } else if (_Data->Form.Nonresident.LowestVcn == 0) {
            length = _Data->Form.Nonresident.FileSize;
        } else {
            length = 0;
        }

        if (length != 0) {

            if (length < AttributeDefTable->QueryMinimumLength(index)) {

                // Length is less than the minimum.

                DbgPrintf("Attribute %d has length less than the minimum.\n", _Data->TypeCode);
                return FALSE;
            }

            // Note that a value of -1 in the Length field of the
            // Attribute Definition Table entry indicates that the
            // attribute can be as large as it pleases.
            //
            if (AttributeDefTable->QueryMaximumLength(index) != -1 &&
                length > AttributeDefTable->QueryMaximumLength(index)) {

                // Length is greater than the maximum.

                DbgPrintf("Attribute %d has length greater than the maximum.\n", _Data->TypeCode);
                return FALSE;
            }
        }
    }


    if (IsResident()) {

        // Make sure that the value is in bounds and
        // make sure that name comes before value.

        if (_Data->Form.Resident.ValueLength) {

            if (_Data->Form.Resident.ValueOffset +
                _Data->Form.Resident.ValueLength >
                _Data->RecordLength) {

                DbgPrintf("Attribute %d has corrupt resident value.\n", _Data->TypeCode);
                return FALSE;
            }

            if (_Data->NameLength &&
                _Data->NameOffset +
                _Data->NameLength >
                _Data->Form.Resident.ValueOffset) {

                DbgPrintf("Attribute %d colliding name and resident value.\n", _Data->TypeCode);
                return FALSE;
            }
        }

        // Make sure that if the attribute is indexed then it
        // has no name.

        if ((_Data->Form.Resident.ResidentFlags & RESIDENT_FORM_INDEXED) &&
            _Data->NameLength) {

            DbgPrintf("Attribute %d is indexed AND has a name.\n", _Data->TypeCode);
            return FALSE;
        }


    } else {

        // Make sure that the mapping pairs are in bounds.

        if (_Data->Form.Nonresident.MappingPairsOffset >=
            _Data->RecordLength) {

            DbgPrintf("Attribute %d has mapping pairs that are out of bounds.\n", _Data->TypeCode);
            return FALSE;
        }


        // Make sure that the name comes before the mapping pairs.

        if (_Data->NameLength &&
            _Data->NameOffset +
            _Data->NameLength >
            _Data->Form.Nonresident.MappingPairsOffset) {

            DbgPrintf("Attribute %d has its name colliding with the mapping pairs.\n", _Data->TypeCode);
            return FALSE;
        }


        // Validate the mapping pairs.

        if (!extent_list.Initialize(_Data->Form.Nonresident.LowestVcn,
                                    (PCHAR) _Data +
                                    _Data->Form.Nonresident.MappingPairsOffset,
                                    _Data->RecordLength -
                                    _Data->Form.Nonresident.MappingPairsOffset,
                                    &bad_mapping_pairs)) {

            DbgPrintf("Attribute %d has bad mapping pairs.\n", _Data->TypeCode);
            return FALSE;
        }

        if (extent_list.QueryNextVcn() !=
            _Data->Form.Nonresident.HighestVcn + 1 && !BeLenient) {

            DbgPrintf("Attribute %d has an invalid highest vcn.\n", _Data->TypeCode);
            return FALSE;
        }


        // If the lowest vcn is 0 then make sure that the three sizes
        // make sense.

        if (_Data->Form.Nonresident.LowestVcn == 0 && !BeLenient) {

            if (_Data->Form.Nonresident.ValidDataLength >
                _Data->Form.Nonresident.FileSize ||
                _Data->Form.Nonresident.FileSize >
                _Data->Form.Nonresident.AllocatedLength) {

                DbgPrintf("Attribute %d has inconsistent sizes.\n", _Data->TypeCode);
                return FALSE;
            }
        }
    }


	// $FILE_NAME attribute must follow additional special structure.

	if (_Data->TypeCode == $FILE_NAME) {

        if (!IsIndexed()) {
            DbgPrintf("File name attribute is not indexed.\n");
			return FALSE;
		}

		file_name = (PFILE_NAME) ((PCHAR) _Data + 
                                  _Data->Form.Resident.ValueOffset);

		value_length = _Data->Form.Resident.ValueLength;

		if (value_length < sizeof(FILE_NAME)) {
            DbgPrintf("Corrupt file name attribute.\n");
			return FALSE;
		}

        if (NtfsFileNameGetLength(file_name) > value_length) {
            DbgPrintf("Corrupt file name attribute.\n");
			return FALSE;
        }


        // Make sure that the file name has no NULL
        // characters in it.  If it does then the attribute
        // is "corrupt".

        for (i = 0; i < file_name->FileNameLength; i++) {
            if (!file_name->FileName[i]) {
                return FALSE;
            }
        }
    }

    return TRUE;
}


BOOLEAN
NTFS_ATTRIBUTE_RECORD::UseClusters(
    IN OUT  PNTFS_BITMAP    VolumeBitmap,
    OUT     PBIG_INT        ClusterCount
    ) CONST
/*++

Routine Description:

    This routine allocates the disk space claimed by this attribute
    record in the bitmap provided.  A check is made to verify that
    the requested disk space is free before the allocation takes
    place.  If the requested space is not available in the bitmap
    then this routine will return FALSE.

Arguments:

    VolumeBitmap    - Supplies the bitmap.
    ClusterCount    - Receives the number of clusters allocated
                      to this record.  Not set if method fails.

Return Value:

    FALSE   - The request bitmap space was not available.
    TRUE    - Success.

--*/
{
    NTFS_EXTENT_LIST    extent_list;
    ULONG               num_extents;
    ULONG               i, j;
    VCN                 next_vcn;
    LCN                 current_lcn;
    BIG_INT             run_length;

    DbgAssert(VolumeBitmap);

    if (IsResident()) {
        *ClusterCount = 0;
        return TRUE;
    }

    if (!QueryExtentList(&extent_list)) {
        return FALSE;
    }

    num_extents = extent_list.QueryNumberOfExtents();

    for (i = 0; i < num_extents; i++) {

        if (!extent_list.QueryExtent(i, &next_vcn, &current_lcn,
                                     &run_length)) {

            DbgAbort("Could not query extent");
            return FALSE;
        }


        // Make sure that the run is free before allocating.
        // If it is not, this indicates a cross-link.

        if (!VolumeBitmap->IsFree(current_lcn, run_length)) {

            DbgPrintf("cross-linked run starts at 0x%X for 0x%X\n",
                      current_lcn.GetLowPart(), run_length.GetLowPart());

            // Free everything so far allocated by this routine.

            for (j = 0; j < i; j++) {

                if (!extent_list.QueryExtent(j, &next_vcn, &current_lcn,
                                             &run_length)) {

                    DbgAbort("Could not query extent");
                    return FALSE;
                }

                VolumeBitmap->SetFree(current_lcn, run_length);
            }

            return FALSE;
        }

        VolumeBitmap->SetAllocated(current_lcn, run_length);
    }

    *ClusterCount = extent_list.QueryClustersAllocated();
    return TRUE;
}


BOOLEAN
NTFS_ATTRIBUTE_RECORD::UseClusters(
    IN OUT  PNTFS_BITMAP    VolumeBitmap,
    OUT     PBIG_INT        ClusterCount,
    IN      ULONG           AllowCrossLinkStart,
    IN      ULONG           AllowCrossLinkLength,
    OUT     PBOOLEAN        DidCrossLinkOccur
    ) CONST
/*++

Routine Description:

    This routine allocates the disk space claimed by this attribute
    record in the bitmap provided.  A check is made to verify that
    the requested disk space is free before the allocation takes
    place.  If the requested space is not available in the bitmap
    then this routine will return FALSE.

    This methode assumes that the range specified by the Allow
    parameters are marked as allocated in the given bitmap.

Arguments:

    VolumeBitmap            - Supplies the bitmap.
    ClusterCount            - Receives the number of clusters allocated
                                to this record.  Not set if method fails.
    AllowCrossLinkStart     - Supplies the start of a range where
                                cross-links are allowed.
    AllowCrossLinkLength    - Supplies the length of the range where
                                cross-links are allowed.
    DidCrossLinkOccur       - Returns whether or not an allowable
                                cross-link occurred.

Return Value:

    FALSE   - The request bitmap space was not available.
    TRUE    - Success.

--*/
{
    BOOLEAN r;

    DbgAssert(DidCrossLinkOccur);

    *DidCrossLinkOccur = FALSE;

    if (UseClusters(VolumeBitmap,ClusterCount)) {
        return TRUE;
    }

    *DidCrossLinkOccur = TRUE;

    VolumeBitmap->SetFree(AllowCrossLinkStart, AllowCrossLinkLength);

    r = UseClusters(VolumeBitmap,ClusterCount);

    VolumeBitmap->SetAllocated(AllowCrossLinkStart, AllowCrossLinkLength);

    return r;
}


BOOLEAN
NTFS_ATTRIBUTE_RECORD::UnUseClusters(
    IN OUT  PNTFS_BITMAP    VolumeBitmap,
    IN      ULONG           LeaveInUseStart,
    IN      ULONG           LeaveInUseLength
    ) CONST
/*++

Routine Description:

    This operation reverses a successful 'UseClusters' operation.

    This method assumes that the LeaveInUse range is already in
    use by the bitmap.

Arguments:

    VolumeBitmap        - Supplies the bitmap.
    LeaveInUseStart     - Supplies the start of the range that this routine
                            should leave in use.
    LeaveInUseLength    - Supplies the length of the range that this
                            routine should leave in use.

Return Value:

    FALSE   - Failure.
    TRUE    - Success.

--*/
{
    NTFS_EXTENT_LIST    extent_list;
    ULONG               num_extents;
    ULONG               i;
    VCN                 next_vcn;
    LCN                 current_lcn;
    BIG_INT             run_length;

    DbgAssert(VolumeBitmap);

    if (IsResident()) {
        return TRUE;
    }

    if (!QueryExtentList(&extent_list)) {
        return FALSE;
    }

    num_extents = extent_list.QueryNumberOfExtents();

    for (i = 0; i < num_extents; i++) {

        if (!extent_list.QueryExtent(i, &next_vcn, &current_lcn,
                                     &run_length)) {

            DbgAbort("Could not query extent");
            return FALSE;
        }

        VolumeBitmap->SetFree(current_lcn, run_length);
    }

    VolumeBitmap->SetAllocated(LeaveInUseStart, LeaveInUseLength);

    return TRUE;
}



NONVIRTUAL
UNTFS_EXPORT
BOOLEAN
NTFS_ATTRIBUTE_RECORD::QueryName(
    OUT PWSTRING    Name
	) CONST
/*++

Routine Description:

	This method returns the name of the attribute.

Arguments:

    Name    - Returns the name of the attribute.

Return Value:

    FALSE   - Failure.
    TRUE    - Success.

--*/
{
    if (FIELD_OFFSET(ATTRIBUTE_RECORD_HEADER, Flags) >= _MaximumLength ||
        _Data->NameOffset + _Data->NameLength > _MaximumLength ||
        _Data->NameLength == 0) {

        return Name->Initialize( "" );

    } else {

        return Name->Initialize((PWSTR)((PBYTE)_Data + _Data->NameOffset),
                                _Data->NameLength);

    }
}


VOID
NTFS_ATTRIBUTE_RECORD::QueryValueLength(
	OUT PBIG_INT ValueLength,
	OUT PBIG_INT AllocatedLength,
	OUT PBIG_INT ValidLength
	) CONST
/*++

Routine Description:

	This method returns the actual, allocated, and valid lengths
	of the attribute value associated with this record.

	If the attribute is resident, these three values are all
	the length of the resident value.

	If the attribute is nonresident, these three values are only
	meaningful if the LowestVcn of this attribute record is 0.

Arguments:

	ValueLength 	-- receives the actual length of the value.
	AllocatedLength -- receives the allocated size of the value.
						(may be NULL if the caller doesn't care)
	ValidLength 	-- receives the valid length of the value.
						(may be NULL if the caller doesn't care)

Return Value:

	None.

--*/
{
	DbgPtrAssert( _Data );

	if( _Data->FormCode == RESIDENT_FORM ) {

		*ValueLength = _Data->Form.Resident.ValueLength;

		if( AllocatedLength != NULL ) {

			*AllocatedLength = _Data->Form.Resident.ValueLength;
		}

		if( ValidLength != NULL ) {

			*ValidLength = _Data->Form.Resident.ValueLength;
		}

	} else {

		DbgAssert( _Data->FormCode == NONRESIDENT_FORM );

		*ValueLength = _Data->Form.Nonresident.FileSize;

		if( AllocatedLength != NULL ) {

			*AllocatedLength = _Data->Form.Nonresident.AllocatedLength;
		}

		if( ValidLength != NULL ) {

			*ValidLength = _Data->Form.Nonresident.ValidDataLength;
		}
	}
}


UNTFS_EXPORT
BOOLEAN
NTFS_ATTRIBUTE_RECORD::QueryExtentList(
    OUT PNTFS_EXTENT_LIST   ExtentList
	) CONST
/*++

Routine Description:

Arguments:

	None.

Return Value:

	A pointer to the extent list.  A return value of NULL indicates
	that the attribute is resident or that an error occurred processing
	the compressed mapping pairs.  (Clients should use IsResident to
	determine whether the attribute value is resident.)

--*/
{
	DbgPtrAssert( _Data );

	if( _Data->FormCode == NONRESIDENT_FORM &&
        ExtentList->Initialize( _Data->Form.Nonresident.LowestVcn,
                                (PVOID)((PBYTE)_Data +
                                    _Data->Form.Nonresident.MappingPairsOffset),
                                 _MaximumLength -
                                    _Data->Form.Nonresident.
                                        MappingPairsOffset ) ) {

        return TRUE;

    } else {

        return FALSE;
	}
}


BOOLEAN
NTFS_ATTRIBUTE_RECORD::IsMatch(
	IN  ATTRIBUTE_TYPE_CODE Type,
	IN  PCWSTRING           Name,
	IN  PCVOID              Value,
	IN  ULONG               ValueLength
	) CONST
/*++

Routine Description:

	This method determines whether the attribute record matches the
	parameters given.

Arguments:

	Type		--	Supplies the type code of the attribute.  This
						is the primary key, and must always be present.
    Name        --  Supplies a name to match.  A name of NULL is the
                        same as specifying the null string.
	Value		--	Supplies the value to match.  If this argument is
						null, any value matches.  Only resident
						attributes can be checked for value matches.
	ValueLength --	Supplies the length of the value (if any).

Notes:

    Value matching is not supported for nonresident attribute values;
    if a Value parameter is supplied, then no non-resident attribute
    records will match.

--*/
{
    DSTRING RecordName;

	DbgPtrAssert( _Data );

	if( Type != _Data->TypeCode ) {

		return FALSE;
	}

	if( Name != NULL ) {

        if( !RecordName.Initialize((PWSTR)((PBYTE)_Data + _Data->NameOffset),
						           _Data->NameLength ) ) {

            return FALSE;
        }

        if( Name->Strcmp( &RecordName ) != 0 ) {

            return FALSE;
        }
    } else if (_Data->NameLength) {
        return FALSE;
    }

	if( Value != NULL &&
		( _Data->FormCode != RESIDENT_FORM ||
		  ValueLength != _Data->Form.Resident.ValueLength ||
		  memcmp( Value,
				  (PBYTE)_Data + _Data->Form.Resident.ValueOffset,
				  (UINT) ValueLength ) ) ) {

		return FALSE;
	}

	return TRUE;
}


LONG
CompareAttributeRecords(
    IN  PCNTFS_ATTRIBUTE_RECORD Left,
    IN  PCNTFS_ATTRIBUTE_RECORD Right,
    IN  PCNTFS_UPCASE_TABLE     UpcaseTable
	)
/*++

Routine Description:

	This method compares two attribute records to determine their
	correct ordering in the File Record Segment.

Arguments:

    Left        --  Supplies the left-hand operand of the comparison.
    Right       --  Supplies the right-hand operand of the comparison.
    UpcaseTable --  Supplies the upcase table for the volume.
                    If this parameter is NULL, name comparison
                    cannot be performed.

Return Value:

    <0 if Left is less than Right
     0 if Left equals Right
    >0 if Left is greater than Right.

Notes:

	Attribute records are ordered first by type code and then
    by name.  An attribute record without a name is less than
    any attribute record of the same type with a name.

    Name comparision is first done case-insensitive; if the names
    are equal by that metric, a case-sensitive comparision is made.

    The UpcaseTable parameter may be omitted if either or both names
    are zero-length, or if they are identical (including case).
    Otherwise, it must be supplied.

--*/
{
    ULONG Result;

    // First, compare the type codes:
    //
    Result = Left->QueryTypeCode() - Right->QueryTypeCode();

    if( Result != 0 ) {

        return Result;
    }

    // They have the same type code, so we have to compare the names.
    // Pass in TRUE for the IsAttribute parameter, to indicate that
    // we are comparing attribute names.
    //
    return( NtfsUpcaseCompare( Left->GetName(),
                               Left->QueryNameLength(),
                               Right->GetName(),
                               Right->QueryNameLength(),
                               UpcaseTable,
                               TRUE ) );
}
