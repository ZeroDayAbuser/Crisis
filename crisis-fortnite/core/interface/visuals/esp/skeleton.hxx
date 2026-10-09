#pragma once

namespace gui {
    namespace esp {

        void skeleton(
            const std::array<
                std::pair< engine::f_vector, engine::f_vector >,
                14 >& pairs,
            color_t color,
            float thickness = 1.5f,
            bool outline = true );

        void skeleton_curved(
            const std::array<
                std::pair< engine::f_vector, engine::f_vector >,
                14 >& pairs,
            color_t color,
            float thickness = 1.5f,
            bool outline = true );

        void skeleton_cpu(
            const std::array<
                std::pair< engine::f_vector, engine::f_vector >,
                14 >& pairs,
            color_t color,
            float thickness = 1.5f,
            bool outline = true );

        void skeleton_curved_cpu(
            const std::array<
                std::pair< engine::f_vector, engine::f_vector >,
                14 >& pairs,
            color_t color,
            float thickness = 1.5f,
            bool outline = true );
        void fingers(
            const std::array<
                std::pair< engine::f_vector, engine::f_vector >,
                38 >& pairs,
            color_t color,
            float thickness = 0.8f,
            bool outline = false );
    }
}
