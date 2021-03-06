/* Requires */
var gulp = require('gulp');
var plumber = require('gulp-plumber');
var concat = require('gulp-concat');
var htmlmin = require('gulp-htmlmin');
var cleancss = require('gulp-clean-css');
var uglifyjs = require('gulp-uglify');
var gzip = require('gulp-gzip');
var del = require('del');

/* HTML Task */
gulp.task('html', function() {
    return gulp.src(['html/*.html', 'html/*.htm'])
        .pipe(plumber())
        .pipe(htmlmin({
            collapseWhitespace: true,
            removeComments: true,
            minifyCSS: true,
            minifyJS: true}))
        .pipe(gzip())
        .pipe(gulp.dest('data'));
});


/* Clean Task */
gulp.task('clean', function() {
    return del(['data/*']);
});

/* Watch Task */
gulp.task('watch', function() {
    gulp.watch('html/*.html', ['html']);
    gulp.watch('html/*.htm', ['html']);
});

/* Default Task */
//gulp.task('default', ['clean', 'html']); old gulp version
gulp.task('default', gulp.series('clean', 'html'));
