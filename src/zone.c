//
// zone.c
//

#include <assert.h>
#include <stdlib.h>
#include <math.h>
#include "item.h"
#include "zone.h"
#include "world.h"
#include "display.h"
#include "inventory.h"
#include "walls.h"

#define ROOM_INFREQ 60
#define ITEM_INFREQ 80
#define CRTR_INFREQ 80
#define ROOM_MIN 60
#define ITEM_MIN 40
#define CRTR_MIN 40

typedef struct {//specifies room with x and y coordinates adn w and h for width and height
	int x, y, w, h;
} room;

typedef struct {
	int xmin,ymin,xmax,ymax;
} partition;

typedef struct {
	int x,y;
} point_t;

typedef struct {
	double radius, angle;
} polar_t;

typedef struct {
	polar_t polar;
	point_t cart;
} ptpair_t;

/* Deprecated
static int in_room(room * r, int x, int y)
{
	return x >= r->x && x < r->x + r->w
		&& y >= r->y && y < r->y + r->h;
}
*/
static int on(zone * z, int x, int y)
{
	if (x < 0) return 1;
	if (y < 0) return 1;
	if (x >= z->width)  return 1;
	if (y >= z->height) return 1;
	return z->tiles[x][y].type & TILE_WALL;
}

static void set_wall_char(zone * z, int x, int y)
{
	int i, j;
	unsigned char ch = 0;

	for (j = 1; j >= -1; j--) {
		for (i = 1; i >= -1; i--) {
			if (i != 0 || j != 0) {
				ch <<= 1;
				ch |= !!on(z, x + i, y + j);
			}
		}
	}

	z->tiles[x][y].ch = wall_chars[ch];
}

/* Deprecated
static void pick_random_subregion(zone * z,partition * p, int * ret)
{
	int x, y;
	partition sqr = {0};

	int edge;

	// Approximately Big
	sqr.xmin = 1000;
	sqr.ymin = 1000;

	for (y = p->ymin; y < p->ymax; y++) {
		for (x = p->xmin; x < p->xmax; x++) {
			// TODO: Optimize this
			if (z->tiles[x][y].type & TILE_FLOOR) {
				if (x < sqr.xmin)
					sqr.xmin = x;
				else if (x > sqr.xmax)
					sqr.xmax = x;

				if (y < sqr.ymin)
					sqr.ymin = y;
				else if (y > sqr.ymax)
					sqr.ymax = y;

			}
		}
	}

	edge = random() % 4;

	fprintf(stderr,"Partition is (%d,%d) to (%d,%d)\n",sqr.xmin,sqr.ymin,sqr.xmax,sqr.ymax);
	fprintf(stderr,"Random edge is: %d, which is %d\n",edge, ((int*) &sqr)[edge]);
	fprintf(stderr,"Picking a random center value between %d <-> %d\n",
		((int*) &sqr)[((edge % 2) + 1) % 4],
		((int*) &sqr)[((edge % 2) + 3) % 4]);



	ret[edge % 2] = ((int*) &sqr)[edge];
	ret[(edge + 1) % 2] = (rand() % abs( ((int*) &sqr)[((edge % 2) + 3) % 4] - ((int*) &sqr)[((edge % 2) + 1) % 4]) ) + ((int*) &sqr)[(edge + 1) % 2]; 

	fprintf(stderr,"New Region center is: (%d,%d)\n\n",ret[0],ret[1]);

}*/


// Get the distance to the nearest edge
// TODO: Optimize this
/* Deprecated
static int nearest_edge(zone * z, int * pos)
{
	int ret;
	ret = pos[0];
	if (pos[1] < ret)
		ret = pos[1];
	if ((z->width - pos[0]) < ret)
		ret = (z->width - pos[0]);
	if ((z->height - pos[1]) < ret)
		ret = (z->height - pos[1]);
	return ret;
} */

static polar_t cart_to_polar(point_t *cart)
{
	polar_t ret;
	
	ret.radius = sqrt((cart->x * cart->x) + (cart->y * cart->y));
	ret.angle = tan( ((float) cart->y) / ((float) cart->x));

	return ret;
}

// OPTIMIZE: Please. Unecessarily memory intensive
static void do_sort(point_t *pts, int num)
{
	int i,j;
	int c;

	ptpair_t *pairs = (ptpair_t*) malloc(sizeof(ptpair_t) * num);
	ptpair_t *sorted_pairs = (ptpair_t*) malloc(sizeof(ptpair_t) * num);

	// Generate pair array
	for (i = 0; i < num; i++) {
		pairs[i].cart = pts[i];
		pairs[i].polar = cart_to_polar(&pts[i]);
	}
	
	
	for (i = 0; i < num; i++) {
		c = 0;
		for (j = 0; j < num; j++) {
			if (pairs[i].polar.radius > pairs[j].polar.radius)
				c++;
		}
		sorted_pairs[c] = pairs[i];
	}

	for (i = 0; i < num; i++) {
		pts[i] = sorted_pairs[i].cart;
	}

	free(pairs);
	free(sorted_pairs);
}


static void radial_sort(point_t * pts, int num)
{
	int i;
	point_t sum = {0};
	point_t avg;


	for (i = 0; i < num; i++) {
		sum.x += pts[i].x;
		sum.y += pts[i].y;
	}

	// Centerpoint for the radial sort
	avg.x = sum.x / num;	
	avg.y = sum.y / num;	

	// Change to the new center
	for (i = 0; i < num; i++) {
		pts[i].x -= avg.x;
		pts[i].y -= avg.y;
	}

	// Sort by angle now
	do_sort(pts,num);

	// Reset the points back to being relative to partition
	for (i = 0; i < num; i++) {
		pts[i].x += avg.x;
		pts[i].y += avg.y;
	}

}

static void dig_room(zone * z, partition * p, point_t *pts, int num)
{
	int i,j;	
	int d;
	int dig = 0;
	int dir; // Toggles between x and y (0 and 1)

	dir = rand() % 2;
	for (i = 0; i < num; i++) {
		// Walk to the next point
		for (d = ((int*) &pts[i])[dir]; d < ((int*) &pts[i+1 % num])[dir]; d++) {
			// Set each of these zone tiles to floor
			z->tiles[pts->x][pts->y].type = TILE_FLOOR;
		}

		dir = !dir;
		
		for (d = ((int*) &pts[i])[dir]; d < ((int*) &pts[i+1 % num])[dir]; d++) {
			z->tiles[pts->x][pts->y].type = TILE_FLOOR;
		}
	}

	/*
	// Scan partition from xmin->xmax, digging floor from wall->wall
	for (j = p->ymin; j < p->ymax; j++) {
		for (i = p->xmin; i < p->xmax; i++) {
			if (z->tiles[i][j].type == TILE_FLOOR)
				dig = !dig;
			
			if (dig) 
				z->tiles[i][j].type = TILE_FLOOR;				
		}
	}*/
}


// Generate a random region in the given boundary
static void generate_region(zone * z, partition * p)
{
	int i;
	// Number of pairs of points
	int num_pts = 4;
	int dir;

	point_t diff;

	diff.x = p->xmax - p->xmin;
	diff.y = p->ymax - p->ymin;

	point_t *pts = (point_t*) malloc(sizeof(point_t) * num_pts * 2);

	// Generate a list of "paired" points
	for (i = 0; i < num_pts; i+=2) {
		dir = rand() % 2;
		pts[i].x = rand() % diff.x;
		pts[i].y = rand() % diff.y;
	
		// Keep one axis constant	
		((int*) &pts[i+1])[(dir + 1) % 2] = ((int*) &pts[i])[(dir + 1) % 2];
		// Vary the other
		((int*) &pts[i+1])[dir] = (((int*) &pts[i])[dir]) + rand() % ((int*) &diff)[dir];
	}

	for (i = 0; i < num_pts; i++) {
		z->tiles[pts[i].x+p->xmin][pts[i].y+p->ymin].ch = 'x';
	}



	radial_sort(pts,num_pts);
	
	for (i = 0; i < num_pts; i++) {
		pts[i].x += p->xmin;
		pts[i].y += p->ymin;
	}


	dig_room(z,p,pts,num_pts);
	free(pts);
}


/*static void generate_region(zone * z, partition * p)
{
	fprintf(stderr,"x: %d -> %d\n",p->xmin,p->xmax);
	fprintf(stderr,"y: %d -> %d\n",p->ymin,p->ymax);
	

	// Find the center of the partition
	//int cx = ((p->xmax - p->xmin) / 2) + p->xmin;
	//int cy = ((p->ymax - p->ymin) / 2) + p->ymin;
	
	int cx = (rand() % (p->xmax - p->xmin - 6)) + p->xmin + 3;
	int cy = (rand() % (p->ymax - p->ymin - 6)) + p->ymin + 3;
	
	fprintf(stderr,"center: (%d,%d)\n\n",cx,cy);

	// Number of subsquares to generate the room
	int squares = 20;//rand() % 5 + 3;

	int i,j,s,rad,temp;
	int r[2];


	// Do the initial starting square	
	for (i = -2; i <= 2; i++) {
		for (j = -2; j <= 2; j++) {
			z->tiles[cx+j][cy+i].type = TILE_FLOOR;
			z->tiles[cx+j][cy+i].ch   = 'X';
		}
	}

	for(s = 0; s < squares; s++) {
		pick_random_subregion(z,p,r); 
		fprintf(stderr,"Random Subregion center: (%d,%d)\n",r[0],r[1]);
		// Picks a random radius for the square from (currently) 0->3, unless an edge is closer
		temp = nearest_edge(z,r);
		if (temp < 3) continue; // Too close to an edge, bailing.

		rad = random() % ((temp < 3) ? temp-1 : 3) + 1;
	
		// TODO: allow rectangles
		for (i = -rad; i <= rad; i++) {
			for (j = -rad; j <=rad; j++) {
				z->tiles[r[0]+j][r[1]+i].type = TILE_FLOOR;
				z->tiles[r[0]+j][r[1]+i].ch   = '.';
			}
		}
	
	}

}*/


static void generate(zone * z)
{
	int xparts,yparts;
	int i,j;
	int x,y;

	// WTF?
	fill_walls();

	fprintf(stderr,"Bounds: %dx%d\n",z->width,z->height);

	// TODO: Randomly pick these numbers
	xparts = yparts = 3;

	// TODO: randomly select partitions (non-deterministic)	
	partition ** parts = malloc(sizeof(partition) * xparts);
	for (i = 0; i < xparts; i++) {
		parts[i] = malloc(sizeof(partition) * yparts);
		for (j = 0; j < yparts; j++) {
			parts[i][j].xmin = (i*z->width)/3;
			parts[i][j].xmax = ((i+1)*z->width)/3;
			parts[i][j].ymin = (j*z->height)/3;
			parts[i][j].ymax = ((j+1)*z->height)/3;
		}
	}

	for (y = 0; y < z->height; y++) {
		for (x = 0; x < z->width; x++) {
			z->tiles[x][y].type = TILE_WALL;
		}
	}

	for (i = 0; i < xparts; i++) {
		for (j = 0; j < yparts; j++) {
			generate_region(z,&parts[i][j]);
		}
	}

	// Set wall character
	for (x = 0; x < z->width; x++) {
		for (y = 0; y < z->height; y++) {
			if (z->tiles[x][y].type & TILE_WALL) {
				z->tiles[x][y].impassible = 1;
				set_wall_char(z,x,y);
			}
		}
	}
}





// this function is really ugly
/*static void generate2(zone * z)
{
	static int first = 1;

	int i, x, y, max;
	int rc;
	int ** walls;
	room * rv;
	item * it;
	creature * cr;

	if (first) {
		fill_walls();
		first = 0;
	}

	// generate rooms
	rc = rand() % ((z->width * z->height) / ROOM_INFREQ) + ROOM_MIN;
	rv = malloc(sizeof(room) * rc);

	for (i = 0; i < rc; i++) {
		rv[i].w = rand() % 12 + 5;
		rv[i].h = rand() % 8 + 3;
		rv[i].x = rand() % (z->width  - rv[i].w - 1) + 1;
		rv[i].y = rand() % (z->height - rv[i].h - 1) + 1;
	}


	// cut out rooms
	walls = malloc(sizeof(int *) * z->width);
	for (x = 0; x < z->width; x++) {
		walls[x] = malloc(sizeof(int) * z->height);

		for (y = 0; y < z->height; y++) {
			for (i = 0; i < rc; i++) {
				if (in_room(rv + i, x, y)) break;
			}
			z->tiles[x][y].ch = '.';
			walls[x][y] = (i == rc);
		}
	}


	// draw walls
	for (x = 0; x < z->width; x++) {
		for (y = 0; y < z->height; y++) {
			if (walls[x][y]) {
				z->tiles[x][y].impassible = 1;
				set_wall_char(walls,z,x,y);
			}
		}
	}

	// place some random junk
	if (world.iform_cnt != 0) {
		max = rand() % (z->width * z->height / ITEM_INFREQ) + ITEM_MIN;
		for (i = max; i >= 0; i--) {
			it = item_new(world.iforms[rand() % world.iform_cnt]);

			do {
				x = rand() % z->width;
				y = rand() % z->height;
			} while (z->tiles[x][y].impassible || !inv_try(z->tiles[x][y].inv, it));

			item_tele(it, x, y, z);
			zone_update(z, x, y);
		}
	}

	// place some more random junk
	if (world.cform_cnt != 0) {
		max = rand() % (z->width * z->height / CRTR_INFREQ) + CRTR_MIN;
		for (i = max; i >= 0; i--) {
			cr = crtr_new(world.cforms[rand() % world.cform_cnt]);

			do {
				x = rand() % z->width;
				y = rand() % z->height;
			} while (z->tiles[x][y].impassible || z->tiles[x][y].crtr != NULL);

			crtr_tele(cr, x, y, z);
			zone_update(z, x, y);
		}
	}
}*/

zone * zone_new(int w, int h)
{
	int i, j;
	zone * z;

	z = malloc(sizeof(zone));
	z->width = w;
	z->height = h;

	z->tiles = malloc(sizeof(tile *) * w);
	for (i = 0; i < w; i++) {
		z->tiles[i] = malloc(sizeof(tile) * h);
		for (j = 0; j < h; j++) {
			z->tiles[i][j].impassible = 0;
			z->tiles[i][j].crtr = NULL;
			z->tiles[i][j].inv = inv_new(TILE_MAX_WEIGHT);
		}
	}
	fprintf(stderr,"Generating\n");
	generate(z);

	return z;
}

void zone_free(zone * z)
{
	int i, j;

	for (i = 0; i < z->width; i++) {
		for (j = 0; j < z->height; j++) {
			// TODO free creature?
			inv_free(z->tiles[i][j].inv);
		}
		free(z->tiles[i]);
	}

	free(z->tiles);
	free(z);
}

void zone_update(zone * z, int x, int y)
{
	int i;
	int weight = -1;
	item * it;
	chtype ch = '.';

	if (z->tiles[x][y].crtr == NULL) {
		for (i = 0; i < z->tiles[x][y].inv->size; i++) {
			it = z->tiles[x][y].inv->itms[i];

			if (it != NULL && it->f->weight > weight) {
				weight = it->f->weight;
				ch = it->f->ch;
			}
		}
	} else {
		ch = z->tiles[x][y].crtr->f->ch;
	}

	z->tiles[x][y].ch = ch;
	mvwaddch(dispscr, y, x, ch);
}

void zone_draw(zone * z)
{
	int i, j;

	for (i = 0; i < z->width; i++) {
		for (j = 0; j < z->height; j++) {
			mvwaddch(dispscr, j, i, z->tiles[i][j].ch);
//			zone_update(z, i, j);
		}
	}

	wrefresh(dispscr);
}

tile * zone_at(zone * z, int x, int y)
{
	if (x < 0 || y < 0 || x >= z->width || y >= z->height) {
		return NULL;
	}
	return &z->tiles[x][y];
}

void zone_step(zone * z, int step)
{
	int x, y;

	for (x = 0; x < z->width; x++) {
		for (y = 0; y < z->height; y++) {
			if (z->tiles[x][y].crtr != NULL) {
				crtr_step(z->tiles[x][y].crtr, step);
			}
		}
	}
}
