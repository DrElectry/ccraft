/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   font.h                                             :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mgama <mgama@student.42lyon.fr>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2024/02/19 14:08:25 by mgama             #+#    #+#             */
/*   Updated: 2024/03/24 20:14:44 by mgama            ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef FONT_H
#define FONT_H

#include <stdint.h>

typedef struct s_img
{
	void		*img;
	char		*addr;
	int			max_addr;
	int			bits_per_pixel;
	int			line_length;
	int			size;
	int			endian;
	int			width;
	int			height;
}				t_img;

typedef struct s_pointi2
{
	int			x;
	int			y;
}				t_pointi2;

typedef struct s_glyph
{
	int			index;
	int16_t		x_min;
	int16_t		y_min;
	int16_t		x_max;
	int16_t		y_max;
	uint16_t	advance_width;
	int16_t		left_side_bearing;
}				t_ttf_glyph;

typedef struct s_true_type_font_family
{
	uint16_t	format;
	char		*font_family;
	char		*font_sub_family;
	char		*full_name;
	char		*postscript_name;
}		t_ttf_familly;

typedef struct s_true_type_font_props
{
	uint32_t	scalar_type;
	uint16_t	num_tables;
	uint16_t	search_range;
	uint16_t	entry_selector;
	uint16_t	range_shift;

	int32_t		version;
	int32_t		font_revision;
	uint32_t	checksum_adjustment;
	uint32_t	magic_number;
	uint16_t	flags;
	uint16_t	units_per_em;
	uint64_t	created;
	uint64_t	modified;
	int16_t		x_min;
	int16_t		y_min;
	int16_t		x_max;
	int16_t		y_max;
	uint16_t	mac_style;
	uint16_t	lowest_rec_ppem;
	int16_t		font_direction_hint;
	int16_t		index_to_loc_format;
	int16_t		glyph_data_format;

	int16_t		ascent;
	int16_t		descent;
	int16_t		line_gap;
	uint16_t	advance_width_max;
	int16_t		min_left_side_bearing;
	int16_t		min_right_side_bearing;
	int16_t		x_max_extent;
	int16_t		caret_slope_rise;
	int16_t		caret_slope_run;
	int16_t		caret_offset;
	int16_t		metric_data_format;
	uint16_t	num_of_long_hor_metrics;
}		t_ttf_props;

typedef struct s_baked_glyph
{
	unsigned short	x0, y0, x1, y1;
	float			xoff, yoff, xadvance;
	float			xoff2, yoff2;
}		t_baked_glyph;

typedef struct s_glyph_quad
{
	float	x0, y0, x1, y1;
	float	s0, t0, s1, t1;
}		t_glyph_quad;

int				create_new_font(const char *path, char *name, const char *style);
int				delete_font(const char *name);

void			*get_font(const char *name);
t_ttf_glyph		*get_glyph(const char *name, char c);
t_ttf_familly	*get_font_family(const char *name);
t_ttf_props		*get_font_props(const char *name);
const char		*get_font_path(const char *name);

uint16_t		text_width(void *font, const char *str, uint32_t size);
uint16_t		text_height(void *font, const char *str, uint32_t size);

int				glyph_index(void *font, uint8_t c);
uint16_t		horizontal_metrics(void *font, uint16_t glyph);
t_pointi2		next_kern(void *font, uint16_t glyph);
t_pointi2		reset_kerns(void *font);

t_ttf_familly	*font_family(void *font);
t_ttf_props		*font_props(void *font);

float			font_pixel_scale(void *font, float pixel_height);
void			font_vmetrics(void *font, float pixel_height, float *ascent, float *descent, float *line_gap);
float			font_kern(void *font, int ch0, int ch1, float pixel_height);
unsigned char	*font_glyph_bitmap(void *font, int codepoint, float pixel_height, int *w, int *h, int *xoff, int *yoff);

int				font_bake(void *font, float pixel_height, unsigned char *pixels, int pw, int ph,
					int first_char, int num_chars, t_baked_glyph *out);
void			font_quad(t_baked_glyph *glyphs, int pw, int ph, int index,
					float *xpos, float *ypos, t_glyph_quad *q, int align);

t_img			*ft_create_string(void *mlx, void *image, const char *text,
					uint32_t size, uint32_t color, const char *font_name, int *width, int *height);
t_img			*ft_new_image(void *mlx, int w, int h);
void			ft_delete_image(void *mlx, t_img **img);
void			ft_pixel_put(t_img *img, int x, int y, uint32_t color);
uint32_t		ft_get_pixel(t_img *img, int x, int y);

#endif /* FONT_H */
