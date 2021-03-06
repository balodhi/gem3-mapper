/*
 *  GEM-Mapper v3 (GEM3)
 *  Copyright (c) 2011-2017 by Santiago Marco-Sola  <santiagomsola@gmail.com>
 *
 *  This file is part of GEM-Mapper v3 (GEM3).
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * PROJECT: GEM-Mapper v3 (GEM3)
 * AUTHOR(S): Santiago Marco-Sola <santiagomsola@gmail.com>
 */

#ifndef NSEARCH_LEVENSHTEIN_SCHEDULED_H_
#define NSEARCH_LEVENSHTEIN_SCHEDULED_H_

#include "utils/essentials.h"
#include "fm_index/fm_index.h"
#include "fm_index/fm_index_query.h"
#include "filtering/region_profile/region_profile.h"
#include "neighborhood_search/dp_matrix.h"
#include "neighborhood_search/nsearch_schedule.h"

/*
 * Levenshtein Scheduled Search
 */
uint64_t nsearch_levenshtein_scheduled_search(
    nsearch_schedule_t* const nsearch_schedule);

#endif /* NSEARCH_LEVENSHTEIN_SCHEDULED_H_ */
