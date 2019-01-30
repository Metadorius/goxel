/* Goxel 3D voxels editor
 *
 * copyright (c) 2015 Guillaume Chereau <guillaume@noctua-software.com>
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

// Parse a gimp palette.
// XXX: we don't check for buffer overflow!
static int parse_gpl(const char *data, char *name, int *columns,
                     palette_entry_t *entries)
{
    const char *start, *end;
    int linen, r, g, b, nb = 0;
    char entry_name[128];
    start = data;

    for (linen = 1, start = data; *start; start = end + 1, linen++) {
        end = strchr(start, '\n');
        if (!end) end = start + strlen(start);

        if (name && sscanf(start, "Name: %[^\n]", name) == 1) {
            name = NULL;
            continue;
        }
        if (columns && sscanf(start, "Columns: %d", columns) == 1) {
            columns = NULL;
            continue;
        }

        if (sscanf(start, "%d %d %d %[^\n]", &r, &g, &b, entry_name) >= 3) {
            if (entries) {
                strcpy(entries[nb].name, entry_name);
                entries[nb].color[0] = r;
                entries[nb].color[1] = g;
                entries[nb].color[2] = b;
                entries[nb].color[3] = 255;
            }
            nb++;
        }
        if (!*end) break;
    }
    return nb;
}

#define READ(type, file) \
    ({ type v; size_t r = fread(&v, sizeof(v), 1, file); (void)r; v;})
static void parse_pal(const char *path, palette_entry_t *entries)
{
    FILE *file;
    file = fopen(path, "rb");
    
    for(int i = 0 ; i<255 ; i++) {
        for(int j = 0 ; j<2 ; j++)
            entries[i].color[j] = READ(uint8_t, file) << 2;
        entries[i].color[3] = 255;
    }
}

static int on_palette(int i, const char *path, void *user)
{
    palette_t **list = user;
    const char *data;
    palette_t *pal;
    pal = calloc(1, sizeof(*pal));
    data = assets_get(path, NULL);
    pal->size = parse_gpl(data, pal->name, &pal->columns, NULL);
    pal->entries = calloc(pal->size, sizeof(*pal->entries));
    parse_gpl(data, NULL, NULL, pal->entries);
    DL_APPEND(*list, pal);
    return 0;
}

static int on_palette2(const char *dir, const char *name, void *user)
{
    palette_t **list = user;
    char *data, *path;
    palette_t *pal;
    asprintf(&path, "%s/%s", dir, name);
    pal = calloc(1, sizeof(*pal));
    if(path[strlen(path) - 2] == 'a') {
        pal->size = 255;
        pal->entries = calloc(pal->size, sizeof(*pal->entries));
        for (int i = 0 ; i < strlen(basename(path)) ; i++)
            pal->name[i] = basename(path)[i];
        parse_pal(path, pal->entries);
    } else if (path[strlen(path) - 2] == 'p') {
        data = read_file(path, NULL);
        pal->size = parse_gpl(data, pal->name, &pal->columns, NULL);
        pal->entries = calloc(pal->size, sizeof(*pal->entries));
        parse_gpl(data, NULL, NULL, pal->entries);
        free(data);
    }
    DL_APPEND(*list, pal);
    free(path);
    return 0;
}


void palette_load_all(palette_t **list)
{
    char *dir;
    FILE *file;
    if ((file = fopen("data/palettes/", "r")))
    {
        fclose(file);
	    sys_list_dir("data/palettes/", on_palette2, list);
    }
    assets_list("data/palettes/", list, on_palette);
    if (sys_get_user_dir()) {
        asprintf(&dir, "%s/palettes", sys_get_user_dir());
        sys_list_dir(dir, on_palette2, list);
        free(dir);
    }
}
