/*
 * Copyright (c) 2016 MariaDB Corporation Ab
 * Copyright (c) 2023 MariaDB plc, Finnish Branch
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file and at www.mariadb.com/bsl11.
 *
 * Change Date: 2028-01-30
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2 or later of the General
 * Public License.
 */

#include <maxscale/ccdefs.hh>
#include "maxavro_internal.hh"
#include <string.h>
#include <maxbase/assert.hh>
#include <maxbase/string.hh>
#include <maxscale/log.hh>
#include <errno.h>

/**
 * @brief Read a single value from a file
 * @param file File to read from
 * @param name Name of the field
 * @param type Type of the field
 * @param field_num Field index in the schema
 * @return JSON object or NULL if an error occurred
 */
static json_t* read_and_pack_value(MAXAVRO_FILE* file,
                                   MAXAVRO_SCHEMA_FIELD* field,
                                   enum maxavro_value_type type)
{
    json_t* value = NULL;
    switch (type)
    {
    case MAXAVRO_TYPE_BOOL:
        if (file->buffer_ptr < file->buffer_end)
        {
            int i = 0;
            memcpy(&i, file->buffer_ptr++, 1);
            value = json_pack("b", i);
        }
        break;

    case MAXAVRO_TYPE_INT:
    case MAXAVRO_TYPE_LONG:
        {
            uint64_t val = 0;
            if (maxavro_read_integer(file, &val))
            {
                json_int_t jsonint = val;
                value = json_pack("I", jsonint);
            }
        }
        break;

    case MAXAVRO_TYPE_ENUM:
        {
            uint64_t val = 0;
            maxavro_read_integer(file, &val);

            json_t* arr = (json_t*)field->extra;
            mxb_assert(arr);
            mxb_assert(json_is_array(arr));

            if (json_array_size(arr) >= val)
            {
                json_t* symbol = json_array_get(arr, val);
                mxb_assert(json_is_string(symbol));
                value = json_pack("s", json_string_value(symbol));
            }
        }
        break;

    case MAXAVRO_TYPE_FLOAT:
        {
            float f = 0;
            if (maxavro_read_float(file, &f))
            {
                double d = f;
                value = json_pack("f", d);
            }
        }

        break;

    case MAXAVRO_TYPE_DOUBLE:
        {
            double d = 0;
            if (maxavro_read_double(file, &d))
            {
                value = json_pack("f", d);
            }
        }
        break;

    case MAXAVRO_TYPE_BYTES:
    case MAXAVRO_TYPE_STRING:
        {
            size_t len;
            char* str = maxavro_read_string(file, &len);
            if (str)
            {
                value = json_stringn(str, len);
                MXB_FREE(str);
            }
        }
        break;

    case MAXAVRO_TYPE_UNION:
        {
            json_t* arr = (json_t*)field->extra;
            uint64_t val = 0;

            if (maxavro_read_integer(file, &val) && val < json_array_size(arr))
            {
                json_t* union_type = json_object_get(json_array_get(arr, val), "type");
                value = read_and_pack_value(file, field, string_to_type(json_string_value(union_type)));
            }
        }
        break;

    case MAXAVRO_TYPE_NULL:
        value = json_null();
        break;

    default:
        MXB_ERROR("Unimplemented type: %d", field->type);
        break;
    }
    return value;
}

static void skip_value(MAXAVRO_FILE* file, enum maxavro_value_type type)
{
    switch (type)
    {
    case MAXAVRO_TYPE_INT:
    case MAXAVRO_TYPE_LONG:
    case MAXAVRO_TYPE_ENUM:
        {
            uint64_t val = 0;
            maxavro_read_integer(file, &val);
        }
        break;

    case MAXAVRO_TYPE_FLOAT:
    case MAXAVRO_TYPE_DOUBLE:
        {
            double d = 0;
            maxavro_read_double(file, &d);
        }
        break;

    case MAXAVRO_TYPE_BYTES:
    case MAXAVRO_TYPE_STRING:
        {
            maxavro_skip_string(file);
        }
        break;

    default:
        MXB_ERROR("Unimplemented type: %d - %s", type, type_to_string(type));
        break;
    }
}

/**
 * @brief Read a record and convert in into JSON
 *
 * @param file File to read from
 * @return JSON value or NULL if an error occurred. The caller must call
 * json_decref() on the returned value to free the allocated memory.
 */
json_t* maxavro_record_read_json(MAXAVRO_FILE* file)
{
    if (!file->metadata_read && !maxavro_read_datablock_start(file))
    {
        return NULL;
    }

    json_t* object = NULL;

    if (file->records_read_from_block < file->records_in_block)
    {
        object = json_object();

        if (object)
        {
            for (size_t i = 0; i < file->schema->num_fields; i++)
            {
                json_t* value = read_and_pack_value(file,
                                                    &file->schema->fields[i],
                                                    file->schema->fields[i].type);
                if (value)
                {
                    json_object_set_new(object, file->schema->fields[i].name, value);
                }
                else
                {
                    long pos = ftell(file->file);
                    MXB_ERROR("Failed to read field value '%s', type '%s' at "
                              "file offset %ld, record number %lu.",
                              file->schema->fields[i].name,
                              type_to_string(file->schema->fields[i].type),
                              pos,
                              file->records_read);
                    json_decref(object);
                    return NULL;
                }
            }
        }

        file->records_read_from_block++;
        file->records_read++;
    }

    return object;
}

static void skip_record(MAXAVRO_FILE* file)
{
    for (size_t i = 0; i < file->schema->num_fields; i++)
    {
        skip_value(file, file->schema->fields[i].type);
    }
    file->records_read_from_block++;
    file->records_read++;
}

/**
 * @brief Read next data block
 *
 * This seeks past any unread data from the current block
 * @param file File to read from
 * @return True if reading the next block was successfully read
 */
bool maxavro_next_block(MAXAVRO_FILE* file)
{
    if (file->last_error == MAXAVRO_ERR_NONE)
    {
        return maxavro_read_datablock_start(file);
    }
    return false;
}

/**
 * @brief Seek to last data block
 *
 * @param file File to seek
 *
 * @return True if the last block was reached successfully
 */
bool maxavro_to_last_block(MAXAVRO_FILE* file)
{
    bool ok = false;
    long pos = 0;

    while (file->last_error == MAXAVRO_ERR_NONE)
    {
        uint64_t records, bytes;

        if (maxavro_read_integer_from_file(file, &records)
            && maxavro_read_integer_from_file(file, &bytes))
        {
            fseek(file->file, bytes + SYNC_MARKER_SIZE, SEEK_CUR);

            if (int c = fgetc(file->file); c != EOF)
            {
                ungetc(c, file->file);
                pos = ftell(file->file);
            }
            else
            {
                fseek(file->file, pos, SEEK_SET);
                ok = maxavro_read_datablock_start(file);
                break;
            }
        }
        else
        {
            break;
        }
    }

    return ok;
}

/**
 * @brief Seek to a position in the Avro file
 *
 * This moves the current position of the file, skipping data blocks if necessary.
 *
 * @param file
 * @param position
 * @return
 */
bool maxavro_record_seek(MAXAVRO_FILE* file, uint64_t offset)
{
    bool rval = true;

    if (offset < file->records_in_block - file->records_read_from_block)
    {
        /** Seek to the end of the block or to the position we want */
        while (offset-- > 0)
        {
            skip_record(file);
        }
    }
    else
    {
        /** We're seeking past a block boundary */
        offset -= (file->records_in_block - file->records_read_from_block);
        maxavro_next_block(file);

        while (offset > file->records_in_block)
        {
            /** Skip full blocks that don't have the position we want */
            offset -= file->records_in_block;
            fseek(file->file, file->buffer_size, SEEK_CUR);
            maxavro_next_block(file);
        }

        mxb_assert(offset <= file->records_in_block);

        while (offset-- > 0)
        {
            skip_record(file);
        }
    }

    return rval;
}

/**
 * @brief seek to file offset
 *
 * This sets the file offset to a position and checks that it is preceeded by a
 * valid sync marker.
 *
 * @param file File to seek
 * @param pos Position in the file to seek to, this should be the starting offset
 * of a data block
 * @return True if seeking to the offset was successful, false if an error occurred
 */
bool maxavro_record_set_pos(MAXAVRO_FILE* file, long pos)
{
    fseek(file->file, pos - SYNC_MARKER_SIZE, SEEK_SET);
    return maxavro_verify_block(file) && maxavro_read_datablock_start(file);
}

/**
 * @brief Read native Avro data
 *
 * This function reads a complete Avro data block from the disk and returns
 * the read data in its native Avro format.
 *
 * @param file File to read from
 * @return Buffer containing the complete binary data block or NULL if an error
 * occurred. Consult maxavro_get_error for more details.
 */
GWBUF maxavro_record_read_binary(MAXAVRO_FILE* file)
{
    GWBUF rval;

    if (file->last_error == MAXAVRO_ERR_NONE)
    {
        if (!file->metadata_read && !maxavro_read_datablock_start(file))
        {
            return rval;
        }

        long data_size = (file->data_start_pos - file->block_start_pos) + file->buffer_size;
        mxb_assert(data_size > 0);
        rval = GWBUF(data_size + SYNC_MARKER_SIZE);

        if (rval)
        {
            auto prev = ftell(file->file);
            fseek(file->file, file->block_start_pos, SEEK_SET);

            if (fread(rval.data(), 1, data_size, file->file) == (size_t)data_size)
            {
                fseek(file->file, prev, SEEK_SET);
                memcpy(rval.data() + data_size, file->sync, sizeof(file->sync));
                maxavro_next_block(file);
            }
            else
            {
                if (ferror(file->file))
                {
                    MXB_ERROR("Failed to read %ld bytes: %d, %s",
                              data_size,
                              errno,
                              mxb_strerror(errno));
                    file->last_error = MAXAVRO_ERR_IO;
                }
                rval.clear();
            }
        }
        else
        {
            MXB_ERROR("Failed to allocate %ld bytes for data block.", data_size);
        }
    }
    else
    {
        MXB_ERROR("Attempting to read from a failed Avro file '%s', error is: %s",
                  file->filename,
                  maxavro_get_error_string(file));
    }
    return rval;
}
