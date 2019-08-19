////////////////////////////////////////////////////////////////////////////////
//
//  File           : block_driver.c
//  Description    : This is the implementation of the standardized IO functions
//                   for used to access the BLOCK storage system.
//
//  Author         : Michael Fox
//

// Includes
#include <stdlib.h>
#include <string.h>

// Project Includes
#include <block_controller.h>
#include <block_driver.h>
#include <block_driver_helper.h>
#include <cmpsc311_log.h>
#include <block_cache.h>

// Global variables
int isOn = 0;
int nbFiles;
int nbHandles;
int freeFrameNr;
file_t files[BLOCK_MAX_TOTAL_FILES];
fh_t handles[BLOCK_MAX_TOTAL_FILES];

//
// Implementation

////////////////////////////////////////////////////////////////////////////////
//
// Function     : block_poweron
// Description  : Startup up the BLOCK interface, initialize filesystem
//
// Inputs       : none
// Outputs      : 0 if successful, -1 if failure

int32_t block_poweron(void)
{
    int i;
    // Check that the device is not already on
    if (isOn) {
        return -1;
    }

    // Call the INITMS opcode
    executeOpcode(NULL, BLOCK_OP_INITMS, 0);
    isOn = 1;
    // Call the BZERO opcode
    
    // Legacy code from assign2. Uncomment below to zero out the block.
    // executeOpcode(NULL, BLOCK_OP_BZERO, 0);

    // Init the data structures
    for (i = 0; i < BLOCK_MAX_TOTAL_FILES; i++) {
	    // memset(&files[i], 0, sizeof(file_t));
	    memset(&handles[i], 0, sizeof(fh_t));
    }    

    //create a buffer to store file metadata
    char * buf;

    for (i = 0; i < BLOCK_MAX_TOTAL_FILES; i++){

	    //allocate buffer memory to size of a frame
	    buf = (char *) malloc(sizeof(frame_t));

	    //read the frames containing metadata into the buffer
	    executeOpcode(buf, BLOCK_OP_RDFRME, i);

	    //copy the buffer into the files struct
	    memcpy(&files[i], buf, sizeof(file_t));

	    //free allocated buffer memory
	    free(buf);
    }	    

    nbHandles = 0;
    freeFrameNr = getFreeFrame(files);
    nbFiles = getNbFiles(files);

    if (init_block_cache() == -1){
	    return -1;
    }

    // Return successfully
    return (0);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : block_poweroff
// Description  : Shut down the BLOCK interface, close all files
//
// Inputs       : none
// Outputs      : 0 if successful, -1 if failure

int32_t block_poweroff(void)
{
    // Check that the device is powered on
    if (!isOn) {
        return -1;
    }

    int i;
    
    //create a char buffer for transfering files metadata
    char * buf;

    if(close_block_cache() == -1){
	    return -1;
    }

    for(i=0; i<BLOCK_MAX_TOTAL_FILES; i++){

	    //allocate memory for buffer equivalent to a frame
	    buf = (char *) malloc(sizeof(frame_t));

	    //copy the data in files struct to the created buffer
	    memcpy(buf, &files[i], sizeof(frame_t));

	    //execute the write command of the buffer to the current frame
	    executeOpcode(buf, BLOCK_OP_WRFRME, i);

	    //free the allocated buffer memory
	    free(buf);
    }


    // Call the POWOFF opcode
    executeOpcode(NULL, BLOCK_OP_POWOFF, 0);
    // Close all files
    closeAllFiles(handles);
    // Free the data structures
    nbFiles = 0;
    nbHandles = 0;
    freeFrameNr = 0;


    // Return successfully
    return (0);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : block_open
// Description  : This function opens the file and returns a file handle
//
// Inputs       : path - filename of the file to open
// Outputs      : file handle if successful, -1 if failure

int16_t block_open(char* path)
{
    int i;
    int found;
    int16_t fd;
    // Check that the device is on
    if (!isOn) {
        return -1;
    }
    // Check if file exists
    found = 0;
    i = 0;
    while (i < nbFiles && !found) {
        if (memcmp(files[i].name, path, strlen(path)) == 0) {
            found = 1;
        } else {
            i++;
        }
    }
    // If no, create/init it
    if (!found) {
        createNewFile(path, &files[nbFiles]);
        i = nbFiles;
        nbFiles++;
    }
    // Open the file
    openFile(&handles[nbHandles], &files[i]);
    fd = nbHandles;
    nbHandles++;
    // THIS SHOULD RETURN A FILE HANDLE
    return (fd);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : block_close
// Description  : This function closes the file
//
// Inputs       : fd - the file descriptor
// Outputs      : 0 if successful, -1 if failure

int16_t block_close(int16_t fd)
{
    // Check that the device is on
    if (!isOn) {
        return -1;
    }
    // Check that fd is a valid file handler (file exists, is open, ...)
    if (handles[fd].status == CLOSED) {
        return -1;
    }
    // Set the file as closed
    closeFile(&handles[fd]);
    // Return successfully
    return (0);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : block_read
// Description  : Reads "count" bytes from the file handle "fh" into the
//                buffer "buf"
//
// Inputs       : fd - filename of the file to read from
//                buf - pointer to buffer to read into
//                count - number of bytes to read
// Outputs      : bytes read if successful, -1 if failure

int32_t block_read(int16_t fd, void* buf, int32_t count)
{
    int32_t remaining;
    int32_t bufOffset;
    int32_t frame_offset;
    int32_t frame_nr;
    int32_t data_size;
    int32_t loc;
    int32_t fileSize;
    frame_t frame;
    file_t* file;
    void* pointer;

    // Check that the device is on
    if (!isOn) {
        return -1;
    }
    // Check that the file handle is correct (file exists, is open, ...)
    if (handles[fd].status == CLOSED) {
        return -1;
    }
    file = handles[fd].file;
    // Make sure we don't read more bytes than we have
    loc = handles[fd].loc;
    fileSize = file->size;
    if (fileSize - loc < count) {
        count = fileSize - loc;
    }
    // While we haven't read `count` or reached the end of the file:
    remaining = count;
    bufOffset = 0;
    while (remaining != 0) {
        frame_offset = loc % BLOCK_FRAME_SIZE;
        frame_nr = file->frames[loc / BLOCK_FRAME_SIZE];

	//check cache for the frame
	pointer = get_block_cache(0, frame_nr);

	//if the frame pointer returned was NULL then execute the opcode
	if(pointer == NULL){
		executeOpcode(frame, BLOCK_OP_RDFRME, frame_nr);
		//place frame in cache
		put_block_cache(0, frame_nr, frame);
	}
	//else copy the frame at pointer
	else{
		memcpy(frame, pointer, BLOCK_FRAME_SIZE);
	}
	///////////////////////////////////////////////////////////////




        //  Copy the relevant contents of the frame over to the buffer
        if (BLOCK_FRAME_SIZE - frame_offset > remaining) {
            data_size = remaining;
        } else {
            data_size = BLOCK_FRAME_SIZE - frame_offset;
        }
        memcpy(buf + bufOffset, frame + frame_offset, data_size);
        bufOffset += data_size;
        loc += data_size;
        remaining -= data_size;
    }
    handles[fd].loc = loc;
    // Return successfully
    return (count);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : block_write
// Description  : Writes "count" bytes to the file handle "fh" from the
//                buffer  "buf"
//
// Inputs       : fd - filename of the file to write to
//                buf - pointer to buffer to write from
//                count - number of bytes to write
// Outputs      : bytes written if successful, -1 if failure

int32_t block_write(int16_t fd, void* buf, int32_t count)
{
    int32_t loc;
    int32_t remaining;
    int32_t frame_offset;
    int32_t frame_nr;
    int32_t bufOffset;
    int32_t data_size;
    file_t* file;
    frame_t frame;
    void* pointer;

    // Check that the file handle is correct (file exists, is open, ...)
    if (handles[fd].status == CLOSED) {
        return -1;
    }
    file = handles[fd].file;
    loc = handles[fd].loc;
    // If needed, add new frames to the file (to allow it to store all the new data)
    if (allocateNewFrames(&handles[fd], count) == -1) {
        return -1;
    }
    remaining = count;
    bufOffset = 0;
    // While we have not written `count`:
    while (remaining > 0) {
        frame_nr = file->frames[loc / BLOCK_FRAME_SIZE];
        frame_offset = loc % BLOCK_FRAME_SIZE;


	//////////////////////////////////////////
	pointer = get_block_cache(0, frame_nr);

	if(pointer == NULL){
		executeOpcode(frame, BLOCK_OP_RDFRME, frame_nr);
	}
	else{
		memcpy(frame, pointer, BLOCK_FRAME_SIZE);
	}
	//////////////////////////////////////////

        //  Copy some of `buf` into the frame buffer
        if (BLOCK_FRAME_SIZE - frame_offset > remaining) {
            data_size = remaining;
        } else {
            data_size = BLOCK_FRAME_SIZE - frame_offset;
        }
        memcpy(frame + frame_offset, (char*)buf + bufOffset, data_size);

        //  Call the WRFRME opcode to write the frame buffer
        executeOpcode(frame, BLOCK_OP_WRFRME, frame_nr);

	put_block_cache(0, frame_nr, frame);
	///////////////////////////////////////////////////

        loc += data_size;
        bufOffset += data_size;
        remaining -= data_size;
    }
    // Return successfully
    handles[fd].loc = loc;
    if(file->size < loc){
	    file->size = loc;
    }
    return (count);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : block_read
// Description  : Seek to specific point in the file
//
// Inputs       : fd - filename of the file to write to
//                loc - offfset of file in relation to beginning of file
// Outputs      : 0 if successful, -1 if failure

int32_t block_seek(int16_t fd, uint32_t loc)
{
    // Check that the file handle is correct (file exists, is open, ...)
    if (handles[fd].status == CLOSED || handles[fd].file->size < loc) {
        return -1;
    }
    // Set the position to the desired location
    handles[fd].loc = loc;
    // Return successfully
    return (0);
}
