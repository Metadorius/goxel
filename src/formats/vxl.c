/* Goxel 3D voxels editor
 *
 * copyright (c) 2016 Guillaume Chereau <guillaume@noctua-software.com>
 *
 * Goxel is free software: you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.

 * Goxel is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.

 * You should have received a copy of the GNU General Public License along with
 * goxel.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "goxel.h"

#define READ(type, file) \
    ({ type v; size_t r = fread(&v, sizeof(v), 1, file); (void)r; v;})
#define WRITE(type, v, file) \
    ({ type v_ = v; fwrite(&v_, sizeof(v_), 1, file);})


//TODO: decrease amount of structs and don't save junk values
typedef struct {
        char fileType[16];
        uint32_t unknown;
        uint32_t numSections;
        uint32_t numSections2;
        uint32_t bodySize;
        uint8_t startPaletteRemap;
        uint8_t endPaletteRemap;
        uint8_t palette[256][3]; 
} header_t;

typedef struct {
        char name[16];
        uint32_t number;
        uint32_t unknown;
        uint32_t unknown2;
} sectionHeader_t;

typedef struct {
        uint8_t skip;
        uint8_t numVoxels; 
} voxelSpanSegment_t;

typedef struct {
        int32_t *spanStart;
        int32_t *spanEnd; 
        voxelSpanSegment_t sections[];
} sectionData_t;

typedef struct {
        uint32_t spanStartOffset;
        uint32_t spanEndOffset;
        uint32_t spanDataOffset;
        float scale;
        float transform[3][4];
        float minBounds[3];
        float maxBounds[3]; 
        uint8_t xSize;
        uint8_t ySize;
        uint8_t zSize;
        uint8_t normalType;
} sectionTailer_t;

typedef struct {
    sectionHeader_t header;
    sectionTailer_t tailer;
    sectionData_t data;
} voxelSection_t;

//TODO: cleanup if possible
static void vxl_import(const char *path) {
    path = path ?: noc_file_dialog_open(NOC_FILE_DIALOG_OPEN, "vxl\0*.vxl\0",
                                        NULL, NULL);
    if (!path) return;
    header_t header;
    FILE *file;
    file = fopen(path, "rb");
    for(int i = 0; i<16 ; i++)
        header.fileType[i] = READ(char, file);

    if(strncmp(header.fileType, "Voxel Animation", 5))
        return;

    header.unknown = READ(uint32_t, file);
    header.numSections = READ(uint32_t, file);
    header.numSections2 = READ(uint32_t, file);
    header.bodySize = READ(uint32_t, file);
    header.startPaletteRemap = READ(uint8_t, file);
    header.endPaletteRemap = READ(uint8_t, file);
    
    for(int x = 0 ; x < 256 ; x++) {
        for(int y = 0 ; y < 3 ; y++) {            
            header.palette[x][y] = READ(uint8_t, file);
        }
    }

    voxelSection_t sections[header.numSections];
    for(int i = 0 ; i < header.numSections ; i++ ) {
        sectionHeader_t header;
        for(int i = 0; i<16 ; i++)
            header.name[i] = READ(char, file);
        header.number = READ(uint32_t, file);
        header.unknown = READ(uint32_t, file);
        header.unknown2 = READ(uint32_t, file);
        sections[i].header = header;
    }
    //Skip to tailer
    fseek(file, 802 + 28 * header.numSections + header.bodySize, 0);   

    for(int i = 0 ; i < header.numSections ; i++) { 
        sectionTailer_t tailer;
        tailer.spanStartOffset = READ(uint32_t, file);
        tailer.spanEndOffset = READ(uint32_t, file);
        tailer.spanDataOffset = READ(uint32_t, file);
        tailer.scale = READ(float, file);
        for(int x = 0 ; x < 3 ; x++) {
            for(int y = 0 ; y < 4 ; y++) {
                tailer.transform[x][y] = READ(float, file);
            }        
        }

        for(int di = 0 ; di < 3 ; di++)
            tailer.minBounds[di] = READ(float, file);

        for(int di = 0 ; di < 3 ; di++)
            tailer.maxBounds[di] = READ(float, file);

        tailer.xSize = READ(uint8_t, file);
        tailer.ySize = READ(uint8_t, file);
        tailer.zSize = READ(uint8_t, file);
        tailer.normalType = READ(uint8_t, file);
        sections[i].tailer = tailer;
    }

    mesh_iterator_t iter = {0};    
    int pos[3];     
    uint8_t colour[4];       
    colour[3] = 255;
    //TODO: put each section in own layer
    for (int i = 0; i < header.numSections; i++) {
        sectionData_t data;
        fseek(file, 802 + 28 * header.numSections + sections[i].tailer.spanStartOffset, 0);
        long n = sections[i].tailer.xSize * sections[i].tailer.ySize;

        data.spanStart = (int32_t*) malloc (n*sizeof(int32_t));
        data.spanEnd = (int32_t*) malloc (n*sizeof(int32_t));

        for (int di = 0 ; di < n ; di++)
            data.spanStart[di] = READ(int32_t, file);

        for (int di = 0 ; di < n ; di++)
            data.spanEnd[di] = READ(int32_t, file);

        long dataStart = ftell(file);

        for (int di = 0; di < n; di++)
        {
            if (data.spanStart[di] == -1)
                continue;
            
            fseek(file, dataStart + data.spanStart[di], 0);
            int x = (short)(di % sections[i].tailer.xSize);
            int y = (short)(di / sections[i].tailer.xSize);
            int z = 0;
            do
            {
                z += READ(uint8_t, file);
                uint8_t count = READ(uint8_t, file);
                for (int j = 0; j < count; j++)
                {
                    uint8_t vColour = READ(uint8_t, file);
                    //TODO: do something with this		
                    //uint8_t vNormal = READ(uint8_t, file);
                    //dump normals for now
                    READ(uint8_t, file);
                    pos[0] = x;
                    pos[1] = y;
                    pos[2] = z;
                    uint8_t *pal = header.palette[vColour];
                    colour[0] = pal[0];
                    colour[1] = pal[1];
                    colour[2] = pal[2];
                    z++;
                    mesh_set_at(goxel.image->active_layer->mesh, &iter, pos, colour);
                }

                READ(uint8_t, file);
            } while (z < sections[i].tailer.zSize);
        }
        free(data.spanStart);
        free(data.spanEnd);
    }   
}

static void export_as_vxl(const char *path) {
    path = path ?: noc_file_dialog_open(NOC_FILE_DIALOG_SAVE,
                    "Westwood vxl\0*.vxl\0", NULL, "untitled.vxl");
    if (!path) return;
    LOG_I("NYI");
    //TODO: Write me
}

ACTION_REGISTER(import_vxl,
    .help = "Import a Westwood voxel file",
    .cfunc = vxl_import,
    .csig = "vp",
    .file_format = {
        .name = "vxl",
        .ext = "*.vxl\0"
    },
)

ACTION_REGISTER(export_as_vxl,
    .help = "Export the image as a Westwood voxel file",
    .cfunc = export_as_vxl,
    .csig = "vp",
    .file_format = {
        .name = "vxl",
        .ext = "*.vxl\0",
    },
)
